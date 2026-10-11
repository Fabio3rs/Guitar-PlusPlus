#pragma once

#include "CTextureImage.h"
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

class CTextureManager;

class CTextureResource {
  public:
    enum class State {
        Queued,
        Decoding,
        ReadyForUpload,
        Ready,
        Failed,
        Cancelled
    };

    struct Snapshot {
        State state = State::Queued;
        unsigned int id = 0;
        int width = 0, height = 0;
        bool decodedAsync = false;
        std::string error;
        std::shared_ptr<const CTextureImage> pixels;
    };

    const std::string path, name, key;

    Snapshot snapshot() const;
    bool isDecoding() const;
    void waitForDecode() const;

  private:
    friend class CTextureManager;
    struct Decoded {
        std::shared_ptr<CTextureImage> image;
        std::string error;
    };

    CTextureResource(std::string directory, std::string filename);
    mutable std::mutex mutex;
    mutable std::condition_variable changed;
    Snapshot status;
    bool keepPixels = false;
    std::shared_ptr<CTextureImage> image;
    std::shared_future<Decoded> future;
};

// Requests/queries may come from multiple threads. Only the owner may load
// synchronously, pump completions or release GPU resources. Call shutdown()
// before losing the GL context; the destructor only joins CPU work.
class CTextureManager {
  public:
    using Handle = std::shared_ptr<CTextureResource>;
    using Completion = std::function<void(const Handle &)>;

    struct Backend {
        // decode may run concurrently. Other operations run on the owner.
        std::function<CTextureImage(const std::string &)> decode;
        // upload must clean up any partially created GPU object on failure.
        std::function<unsigned int(CTextureImage &)> upload;
        std::function<void(unsigned int)> destroy;
        std::function<void(const std::string &)> reportError;
    };

    explicit CTextureManager(Backend backend, size_t capacity = 200);
    ~CTextureManager();
    CTextureManager(const CTextureManager &) = delete;
    CTextureManager &operator=(const CTextureManager &) = delete;

    // A null handle means rejection (full queue or shutdown). Terminal cache
    // hits call completion immediately only on the owner, outside all locks.
    Handle requestAsync(const std::string &path, const std::string &name,
                        Completion completion = {}, bool keepPixels = false);
    // Completes this resource only: no event pumping or callback dispatch.
    // Synchronous failures may be retried; asynchronous failures stay cached,
    // preserving the legacy facade's different failure contracts.
    Handle loadSync(const std::string &path, const std::string &name,
                    bool keepPixels = false);
    Handle find(const std::string &key) const;
    size_t pendingCount() const;
    void pump(size_t limit = 50);
    void drain();
    void shutdown();
    bool closed() const;
    bool isOwnerThread() const;

  private:
    struct Request {
        Handle resource;
        Completion completion;
    };

    const Backend backend;
    const size_t capacity;
    const std::thread::id owner;
    mutable std::mutex mutex;
    std::condition_variable changed;
    std::map<std::string, Handle> cache;
    std::deque<Request> pending;
    size_t active = 0;
    bool stopping = false, force = false;

    static CTextureResource::Decoded
    decode(const std::function<CTextureImage(const std::string &)> &loader,
           const std::string &key, bool emptyName);
    Handle entry(const std::string &path, const std::string &name);
    void startAsync(const Handle &resource);
    bool finish(const Handle &resource, bool wait);
    void publish(const Handle &resource, CTextureResource::Decoded decoded);
    void dispatch(const Completion &completion, const Handle &resource) const;
    void stop(bool releaseGPU);
    void requireOwner() const;
};
