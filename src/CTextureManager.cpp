#include "CTextureManager.h"
#include <chrono>
#include <stdexcept>
#include <utility>
#include <vector>

using State = CTextureResource::State;

CTextureResource::CTextureResource(std::string directory, std::string filename)
    : path(std::move(directory)), name(std::move(filename)),
      key(path + "/" + name) {}

CTextureResource::Snapshot CTextureResource::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex);
    auto result = status;
    if (status.state == State::Ready && keepPixels) {
        result.pixels = image;
    }
    return result;
}

bool CTextureResource::isDecoding() const {
    std::lock_guard<std::mutex> lock(mutex);
    return status.state == State::Decoding &&
           (!future.valid() || future.wait_for(std::chrono::seconds(0)) !=
                                   std::future_status::ready);
}

void CTextureResource::waitForDecode() const {
    std::shared_future<Decoded> work;
    {
        std::unique_lock<std::mutex> lock(mutex);
        changed.wait(lock, [this] {
            return status.state != State::Decoding || future.valid();
        });
        work = future;
    }
    if (work.valid()) {
        work.wait();
    }
}

CTextureManager::CTextureManager(Backend operations, size_t queueCapacity)
    : backend(std::move(operations)), capacity(queueCapacity),
      owner(std::this_thread::get_id()) {}

CTextureManager::~CTextureManager() { stop(false); }

bool CTextureManager::isOwnerThread() const {
    return owner == std::this_thread::get_id();
}

void CTextureManager::requireOwner() const {
    if (!isOwnerThread()) {
        throw std::logic_error("Texture publication requires the owner thread");
    }
}

CTextureResource::Decoded CTextureManager::decode(
    const std::function<CTextureImage(const std::string &)> &loader,
    const std::string &key, bool emptyName) {
    try {
        if (emptyName) {
            return {nullptr, "Empty texture name"};
        }
        auto image = std::make_shared<CTextureImage>(loader(key));
        if (!image->Data || image->Width <= 0 || image->Height <= 0) {
            return {nullptr, "Texture decoder returned an empty image: " + key};
        }
        return {std::move(image), {}};
    } catch (const std::exception &e) {
        return {nullptr, e.what()};
    } catch (...) {
        return {nullptr, "Unknown texture decoder failure: " + key};
    }
}

// Caller holds the manager mutex. Resource objects never move or leave the
// cache during its lifetime; handles can outlive shutdown in Cancelled state.
CTextureManager::Handle CTextureManager::entry(const std::string &path,
                                               const std::string &name) {
    auto &resource = cache[path + "/" + name];
    if (!resource) {
        resource = Handle(new CTextureResource(path, name));
    }
    return resource;
}

// Caller holds both manager and resource mutexes (in that order). The worker
// captures only owned inputs and returns its payload through the future.
void CTextureManager::startAsync(const Handle &resource) {
    resource->status = {};
    resource->status.state = State::Decoding;
    resource->status.decodedAsync = true;
    try {
        resource->future =
            std::async(std::launch::async, [loader = backend.decode,
                                            key = resource->key,
                                            emptyName =
                                                resource->name.empty()] {
                return decode(loader, key, emptyName);
            }).share();
    } catch (const std::exception &e) {
        resource->status.state = State::Failed;
        resource->status.error = e.what();
    }
    resource->changed.notify_all();
}

CTextureManager::Handle CTextureManager::requestAsync(const std::string &path,
                                                      const std::string &name,
                                                      Completion completion,
                                                      bool retainPixels) {
    Handle resource;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (stopping) {
            return {};
        }
        const auto found = cache.find(path + "/" + name);
        if (found == cache.end() && pending.size() + active >= capacity) {
            return {};
        }
        resource = entry(path, name);
        std::lock_guard<std::mutex> resourceLock(resource->mutex);
        auto &status = resource->status;
        const bool terminal =
            status.state == State::Ready ||
            (status.state == State::Failed && status.decodedAsync);
        if (terminal && (!completion || isOwnerThread())) {
            resource->keepPixels = resource->keepPixels || retainPixels;
        } else {
            if (!completion) {
                for (const auto &request : pending) {
                    if (request.resource == resource && !request.completion) {
                        resource->keepPixels =
                            resource->keepPixels || retainPixels;
                        return resource;
                    }
                }
            }
            if (pending.size() + active >= capacity) {
                return {};
            }
            pending.push_back({resource, std::move(completion)});
            resource->keepPixels = resource->keepPixels || retainPixels;
            if (status.state == State::Queued ||
                (status.state == State::Failed && !status.decodedAsync)) {
                startAsync(resource);
            }
            return resource;
        }
    }
    dispatch(completion, resource);
    return resource;
}

CTextureManager::Handle CTextureManager::loadSync(const std::string &path,
                                                  const std::string &name,
                                                  bool retainPixels) {
    requireOwner();
    Handle resource;
    bool decodeHere = false;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (stopping) {
            return {};
        }
        resource = entry(path, name);
        std::lock_guard<std::mutex> resourceLock(resource->mutex);
        resource->keepPixels = resource->keepPixels || retainPixels;
        if (resource->status.state == State::Queued ||
            (resource->status.state == State::Failed &&
             !resource->status.decodedAsync)) {
            resource->status = {};
            resource->status.state = State::Decoding;
            decodeHere = true;
        }
    }
    if (decodeHere) {
        publish(resource, decode(backend.decode, resource->key, name.empty()));
    }
    finish(resource, true);
    {
        std::lock_guard<std::mutex> lock(mutex);
        // Silent prefetch tickets need no callback dispatch. In particular a
        // blocking batch must not leave its own completed prefetches pending.
        for (auto it = pending.begin(); it != pending.end();) {
            if (it->resource == resource && !it->completion) {
                it = pending.erase(it);
            } else {
                ++it;
            }
        }
        if (pending.empty() && active == 0) {
            force = false;
            changed.notify_all();
        }
    }
    return resource;
}

void CTextureManager::publish(const Handle &resource,
                              CTextureResource::Decoded decoded) {
    std::lock_guard<std::mutex> lock(resource->mutex);
    resource->future = {};
    resource->image = std::move(decoded.image);
    resource->status.error = std::move(decoded.error);
    if (resource->image) {
        resource->status.width = resource->image->Width;
        resource->status.height = resource->image->Height;
        resource->status.state = State::ReadyForUpload;
    } else {
        resource->status.state = State::Failed;
    }
    resource->changed.notify_all();
}

bool CTextureManager::finish(const Handle &resource, bool wait) {
    std::shared_future<CTextureResource::Decoded> work;
    {
        std::lock_guard<std::mutex> lock(resource->mutex);
        if (resource->status.state == State::Decoding) {
            work = resource->future;
        }
    }
    if (work.valid()) {
        if (!wait && work.wait_for(std::chrono::seconds(0)) !=
                         std::future_status::ready) {
            return false;
        }
        publish(resource, work.get());
        work = {};
    }

    std::shared_ptr<CTextureImage> image;
    {
        std::lock_guard<std::mutex> lock(resource->mutex);
        if (resource->status.state != State::ReadyForUpload) {
            return resource->status.state == State::Ready ||
                   resource->status.state == State::Failed ||
                   resource->status.state == State::Cancelled;
        }
        image = resource->image;
    }

    unsigned int id = 0;
    std::string error;
    try {
        id = backend.upload(*image);
        if (id == 0) {
            error = "Texture upload failed: " + resource->key;
        }
    } catch (const std::exception &e) {
        error = e.what();
    } catch (...) {
        error = "Unknown texture upload failure: " + resource->key;
    }
    {
        std::lock_guard<std::mutex> lock(resource->mutex);
        resource->status.id = id;
        resource->status.error = std::move(error);
        resource->status.state = (id != 0u) ? State::Ready : State::Failed;
        if (!resource->keepPixels || (id == 0u)) {
            resource->image.reset();
        }
    }
    return true;
}

void CTextureManager::dispatch(const Completion &completion,
                               const Handle &resource) const {
    if (!completion) {
        return;
    }
    try {
        completion(resource);
    } catch (...) {
        if (backend.reportError) {
            try {
                backend.reportError("Texture completion callback failed: " +
                                    resource->key);
            } catch (...) {
                // Diagnostic failure must not abandon the remaining requests.
            }
        }
    }
}

void CTextureManager::pump(size_t limit) {
    requireOwner();
    size_t scans = 0;
    {
        std::lock_guard<std::mutex> lock(mutex);
        scans = pending.size();
    }
    size_t completed = 0;
    while (scans-- > 0) {
        Request request;
        bool wait = false;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (stopping || pending.empty() || (!force && completed >= limit)) {
                break;
            }
            wait = force;
            request = std::move(pending.front());
            pending.pop_front();
            ++active;
        }
        const bool done = finish(request.resource, wait);
        if (done) {
            dispatch(request.completion, request.resource);
            ++completed;
        }
        {
            std::lock_guard<std::mutex> lock(mutex);
            --active;
            if (!done && !stopping) {
                pending.push_back(std::move(request));
            }
            if (pending.empty() && active == 0) {
                force = false;
                changed.notify_all();
            }
        }
    }
}

void CTextureManager::drain() {
    if (!isOwnerThread()) {
        std::unique_lock<std::mutex> lock(mutex);
        if (!stopping && (!pending.empty() || active != 0)) {
            force = true;
            changed.wait(lock, [this] {
                return stopping || (pending.empty() && active == 0);
            });
        }
        return;
    }
    for (;;) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            // The current callback is already outside the queue, so a
            // reentrant drain never waits for its own stack frame.
            if (stopping || pending.empty()) {
                return;
            }
            force = true;
        }
        pump();
    }
}

CTextureManager::Handle CTextureManager::find(const std::string &key) const {
    std::lock_guard<std::mutex> lock(mutex);
    const auto found = cache.find(key);
    return found == cache.end() ? Handle{} : found->second;
}

size_t CTextureManager::pendingCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    return pending.size() + active;
}

bool CTextureManager::closed() const {
    std::lock_guard<std::mutex> lock(mutex);
    return stopping;
}

void CTextureManager::shutdown() {
    requireOwner();
    stop(true);
}

void CTextureManager::stop(bool releaseGPU) {
    std::vector<Handle> resources;
    std::deque<Request> discarded;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (stopping) {
            return;
        }
        stopping = true;
        discarded.swap(pending);
        for (const auto &item : cache) {
            resources.push_back(item.second);
        }
        changed.notify_all();
    }
    for (const auto &resource : resources) {
        resource->waitForDecode();
        unsigned int id = 0;
        {
            std::lock_guard<std::mutex> lock(resource->mutex);
            id = resource->status.id;
            resource->status.id = 0;
            resource->status.state = State::Cancelled;
            resource->status.error = "Texture manager shut down";
            resource->future = {};
            resource->image.reset();
            resource->changed.notify_all();
        }
        if (releaseGPU && (id != 0u)) {
            backend.destroy(id);
        }
    }
}
