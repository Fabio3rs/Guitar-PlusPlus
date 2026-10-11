#pragma once

#include "CModelData.h"
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>

// CPU jobs own their inputs. GPU publication and dependencies run only on
// owner. Handles never own GPU objects. Publishers must not retain borrowed
// targets.
class CModelManager {
    struct Job;

  public:
    enum class State {
        Decoding,
        ReadyForUpload,
        WaitingForTextures,
        Ready,
        Failed,
        Cancelled
    };
    struct Snapshot {
        State state = State::Decoding;
        std::shared_ptr<const CModelData> data;
        std::string error;
        bool terminal() const {
            return state == State::Ready || state == State::Failed ||
                   state == State::Cancelled;
        }
    };
    using Publisher = std::function<bool(const CModelData &)>;
    struct Resource {
        Snapshot snapshot() const;

      private:
        friend class CModelManager;
        mutable std::mutex mutex;
        Snapshot status;
        std::shared_ptr<Job> job;
        Publisher publish;
        std::weak_ptr<int> manager;
        bool finishing = false;
    };
    using Handle = std::shared_ptr<Resource>;
    using Decoder = std::function<CModelData(const CModelRequest &)>;
    // false defers publication; wait=true must complete dependencies or throw.
    using Dependencies = std::function<bool(const CModelData &, bool wait)>;

    explicit CModelManager(Decoder decode = readModelData,
                           Dependencies dependencies = {},
                           size_t capacity = 200);
    ~CModelManager();
    CModelManager(const CModelManager &) = delete;
    CModelManager &operator=(const CModelManager &) = delete;

    Handle requestAsync(CModelRequest request, Publisher publish = {});
    // Without a publisher, completion means CPU ReadyForUpload (legacy load).
    // Sync requests may bypass queue capacity, and never pump unrelated jobs.
    Handle loadSync(CModelRequest request, Publisher publish = {});
    // Owner-only: attach publication immediately after reserving a CPU request,
    // before pumping it. Useful when acquiring a target invalidates older work.
    void setPublisher(const Handle &handle, Publisher publish);
    // Own this reference across callbacks: callers may reset their handle.
    bool finish(Handle handle, bool wait = true);
    void pump(size_t limit = 2);
    void cancel(const Handle &handle);
    void shutdown();
    size_t pendingCount() const;
    bool closed() const;
    bool isOwnerThread() const;

  private:
    const Decoder decoder;
    const Dependencies dependencies;
    const size_t capacity;
    const std::thread::id owner;
    std::shared_ptr<int> identity = std::make_shared<int>(0);
    mutable std::mutex mutex;
    std::map<CModelRequest, std::shared_ptr<Job>> jobs;
    std::deque<Handle> pending;
    bool stopping = false;

    Handle request(CModelRequest request, Publisher publish, bool async);
    void requireOwner() const;
    void retire(const Handle &handle);
    void stop();
    void pruneJobs(); // manager mutex held
};
