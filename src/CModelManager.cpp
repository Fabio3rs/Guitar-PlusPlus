#include "CModelManager.h"
#include <algorithm>
#include <chrono>
#include <stdexcept>

struct CModelManager::Job {
    struct Result {
        std::shared_ptr<const CModelData> data;
        std::string error;
    };
    std::shared_future<Result> future;
};

CModelManager::Snapshot CModelManager::Resource::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex);
    return status;
}

CModelManager::CModelManager(Decoder decode, Dependencies ready, size_t limit)
    : decoder(std::move(decode)), dependencies(std::move(ready)),
      capacity(limit), owner(std::this_thread::get_id()) {}

CModelManager::~CModelManager() { stop(); }

bool CModelManager::isOwnerThread() const {
    return owner == std::this_thread::get_id();
}

void CModelManager::requireOwner() const {
    if (!isOwnerThread()) {
        throw std::logic_error("Model publication requires its owner thread");
    }
}

void CModelManager::pruneJobs() {
    // Retain only jobs still in use or running. A failed load is retryable.
    for (auto it = jobs.begin(); it != jobs.end();) {
        if (it->second.use_count() == 1 &&
            it->second->future.wait_for(std::chrono::seconds(0)) ==
                std::future_status::ready) {
            it = jobs.erase(it);
        } else {
            ++it;
        }
    }
}

CModelManager::Handle CModelManager::request(CModelRequest description,
                                             Publisher publish, bool async) {
    // Keep the supplied path semantics. Lexically collapsing link/../file
    // can select another file; only identical request spellings are coalesced.
    auto handle = std::make_shared<Resource>();
    handle->publish = std::move(publish);
    handle->manager = identity;
    std::unique_lock<std::mutex> lock(mutex);
    pruneJobs();
    if (stopping || (async && pending.size() >= capacity)) {
        return {};
    }
    auto found = jobs.find(description);
    if (found != jobs.end()) {
        handle->job = found->second;
    } else {
        if (async && jobs.size() >= capacity) {
            return {};
        }
        auto decode = [loader = decoder, description]() -> Job::Result {
            try {
                return {std::make_shared<const CModelData>(loader(description)),
                        {}};
            } catch (const std::exception &error) {
                return {{}, error.what()};
            } catch (...) {
                return {{}, "Unknown model decoder failure"};
            }
        };
        auto job = std::make_shared<Job>();
        // Deferred work runs outside locks when finish() requests it.
        try {
            job->future =
                std::async(async ? std::launch::async : std::launch::deferred,
                           std::move(decode))
                    .share();
        } catch (const std::exception &error) {
            handle->status = {State::Failed, {}, error.what()};
            return handle;
        }
        jobs.emplace(std::move(description), job);
        handle->job = std::move(job);
    }
    pending.push_back(handle);
    return handle;
}

CModelManager::Handle CModelManager::requestAsync(CModelRequest request,
                                                  Publisher publish) {
    return this->request(std::move(request), std::move(publish), true);
}

CModelManager::Handle CModelManager::loadSync(CModelRequest request,
                                              Publisher publish) {
    requireOwner();
    auto handle = this->request(std::move(request), std::move(publish), false);
    if (handle) {
        finish(handle);
    }
    return handle;
}

void CModelManager::setPublisher(const Handle &handle, Publisher publish) {
    requireOwner();
    if (!handle || handle->manager.lock() != identity) {
        throw std::invalid_argument("Foreign or null model handle");
    }
    std::lock_guard<std::mutex> lock(handle->mutex);
    if (handle->status.state != State::Decoding || handle->finishing) {
        throw std::logic_error(
            "Model publication must be attached before processing");
    }
    handle->publish = std::move(publish);
}

void CModelManager::retire(const Handle &handle) {
    std::lock_guard<std::mutex> lock(mutex);
    pending.erase(std::remove(pending.begin(), pending.end(), handle),
                  pending.end());
}

bool CModelManager::finish(Handle handle, bool wait) {
    requireOwner();
    if (!handle || handle->manager.lock() != identity) {
        throw std::invalid_argument("Foreign or null model handle");
    }
    std::shared_ptr<Job> job;
    Publisher publish;
    {
        std::lock_guard<std::mutex> lock(handle->mutex);
        if (handle->status.terminal() ||
            (!handle->job && handle->status.data)) {
            return true;
        }
        if (handle->finishing) {
            return false; // A publisher may request/pump other resources.
        }
        job = handle->job;
        if (!wait && job->future.wait_for(std::chrono::seconds(0)) !=
                         std::future_status::ready) {
            return false;
        }
        handle->finishing = true;
        publish = handle->publish;
    }
    Snapshot result;
    bool done = true;
    try {
        const auto decoded = job->future.get();
        result = {decoded.data ? State::ReadyForUpload : State::Failed,
                  decoded.data, decoded.error};
        if (decoded.data && publish) {
            // Publish the CPU phase before checking asynchronous dependencies.
            {
                std::lock_guard<std::mutex> lock(handle->mutex);
                handle->status = result;
            }
            if (dependencies && !dependencies(*decoded.data, wait)) {
                if (wait) {
                    throw std::runtime_error(
                        "Model dependencies did not complete");
                }
                result.state = State::WaitingForTextures;
                done = false;
            } else if (handle->snapshot().state == State::Cancelled) {
                result = {State::Cancelled, {}, {}};
            } else {
                result.state =
                    publish(*decoded.data) ? State::Ready : State::Cancelled;
                result.data.reset();
            }
        }
    } catch (const std::exception &error) {
        result = {State::Failed, {}, error.what()};
    } catch (...) {
        result = {State::Failed, {}, "Unknown model publication failure"};
    }
    {
        std::lock_guard<std::mutex> lock(handle->mutex);
        if (handle->status.state != State::Cancelled) {
            handle->status = std::move(result);
        } else {
            done = true;
        }
        handle->finishing = false;
        if (done) {
            handle->job.reset();
            handle->publish = {};
        }
    }
    if (done) {
        retire(handle);
    }
    return done;
}

void CModelManager::pump(size_t limit) {
    requireOwner();
    std::deque<Handle> work;
    {
        std::lock_guard<std::mutex> lock(mutex);
        work = pending;
    }
    for (const auto &handle : work) {
        if (limit == 0) {
            break;
        }
        if (finish(handle, false)) {
            --limit;
        }
    }
    std::lock_guard<std::mutex> lock(mutex);
    pruneJobs();
}

void CModelManager::cancel(const Handle &handle) {
    requireOwner();
    if (!handle || handle->manager.lock() != identity) {
        throw std::invalid_argument("Foreign or null model handle");
    }
    {
        std::lock_guard<std::mutex> lock(handle->mutex);
        if (handle->status.terminal()) {
            return;
        }
        handle->status = {State::Cancelled, {}, {}};
        handle->publish = {};
        // jobs owns running futures until pruning/shutdown.
        handle->job.reset();
    }
    retire(handle);
}

void CModelManager::stop() {
    std::deque<Handle> abandoned;
    std::map<CModelRequest, std::shared_ptr<Job>> work;
    {
        std::lock_guard<std::mutex> lock(mutex);
        stopping = true;
        abandoned.swap(pending);
        work.swap(jobs);
    }
    for (const auto &handle : abandoned) {
        std::lock_guard<std::mutex> lock(handle->mutex);
        handle->status = {State::Cancelled, {}, {}};
        handle->publish = {};
        handle->job.reset();
    }
    // No locks or callbacks while joining. Deferred jobs need not be executed.
    for (const auto &entry : work) {
        if (entry.second->future.wait_for(std::chrono::seconds(0)) !=
            std::future_status::deferred) {
            entry.second->future.wait();
        }
    }
}

void CModelManager::shutdown() {
    requireOwner();
    stop();
}

size_t CModelManager::pendingCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    return pending.size();
}

bool CModelManager::closed() const {
    std::lock_guard<std::mutex> lock(mutex);
    return stopping;
}
