#include "CTextureManager.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <gtest/gtest.h>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using State = CTextureResource::State;

CTextureImage pixel() {
    CTextureImage result;
    result.Width = result.Height = 1;
    result.BytesPerPixel = 4;
    result.DataBytes = 4;
    result.Data = std::make_unique<unsigned char[]>(4);
    return result;
}

struct FakeGPU {
    std::atomic<int> decodes{0}, uploads{0}, destroys{0}, errors{0};
    const std::thread::id owner = std::this_thread::get_id();
    CTextureManager::Backend backend() {
        return {[this](const std::string &) {
                    ++decodes;
                    return pixel();
                },
                [this](CTextureImage &) {
                    EXPECT_EQ(std::this_thread::get_id(), owner);
                    return static_cast<unsigned int>(++uploads);
                },
                [this](unsigned int id) {
                    EXPECT_EQ(std::this_thread::get_id(), owner);
                    EXPECT_NE(id, 0U);
                    ++destroys;
                },
                [this](const std::string &) { ++errors; }};
    }
};

class Gate {
    std::mutex mutex;
    std::condition_variable changed;
    size_t arrivals = 0;
    bool opened = false;

  public:
    void enter() {
        std::unique_lock<std::mutex> lock(mutex);
        ++arrivals;
        changed.notify_all();
        changed.wait(lock, [this] { return opened; });
    }

    bool started(size_t count = 1) {
        std::unique_lock<std::mutex> lock(mutex);
        return changed.wait_for(lock, std::chrono::seconds(5),
                                [this, count] { return arrivals >= count; });
    }

    void open() {
        std::lock_guard<std::mutex> lock(mutex);
        opened = true;
        changed.notify_all();
    }
};

// Declare after the manager, so fatal assertions also release blocked decoders
// before its destructor joins them.
struct OpenOnExit {
    Gate &gate;
    ~OpenOnExit() { gate.open(); }
};

TEST(TextureManager, SynchronousLoadIsReadyWithoutPumpingAndReusesIdentity) {
    FakeGPU gpu;
    CTextureManager manager(gpu.backend());
    auto resource = manager.loadSync("images", "one");
    ASSERT_NE(resource, nullptr);
    EXPECT_EQ(resource->snapshot().state, State::Ready);
    EXPECT_NE(resource->snapshot().id, 0U);
    EXPECT_EQ(resource->snapshot().width, 1);
    EXPECT_EQ(manager.loadSync("images", "one"), resource);
    EXPECT_EQ(manager.find("images/one"), resource);
    EXPECT_EQ(manager.find("images/missing"), nullptr);
    EXPECT_EQ(gpu.decodes, 1);
    EXPECT_EQ(gpu.uploads, 1);
    EXPECT_EQ(manager.pendingCount(), 0U);
    manager.shutdown();
    manager.shutdown();
    EXPECT_EQ(gpu.destroys, 1);
    EXPECT_EQ(resource->snapshot().state, State::Cancelled);
    EXPECT_EQ(resource->snapshot().id, 0U);
}

TEST(TextureManager,
     ReadyCacheCallbackIsImmediateAndCanRequestAnotherResource) {
    FakeGPU gpu;
    CTextureManager manager(gpu.backend());
    auto first = manager.loadSync("images", "one");
    int callbacks = 0;
    auto again = manager.requestAsync("images", "one", [&](const auto &ready) {
        EXPECT_EQ(ready, first);
        EXPECT_EQ(manager.find("images/one"), first);
        EXPECT_NE(manager.requestAsync("images", "two"), nullptr);
        ++callbacks;
    });
    EXPECT_EQ(again, first);
    EXPECT_EQ(callbacks, 1);
    manager.drain();
    EXPECT_EQ(gpu.uploads, 2);
    manager.shutdown();
}

TEST(TextureManager, ConcurrentRequestsDecodeOnceAndSyncPreservesEachCallback) {
    FakeGPU gpu;
    Gate gate;
    auto backend = gpu.backend();
    backend.decode = [&](const std::string &) {
        ++gpu.decodes;
        gate.enter();
        return pixel();
    };
    CTextureManager manager(std::move(backend));
    OpenOnExit release{gate};
    constexpr size_t count = 12;
    std::vector<CTextureManager::Handle> handles(count);
    std::vector<std::thread> producers;
    int callbacks = 0;
    for (size_t i = 0; i < count; ++i) {
        producers.emplace_back([&, i] {
            handles[i] =
                manager.requestAsync("images", "one", [&](const auto &ready) {
                    EXPECT_EQ(std::this_thread::get_id(), gpu.owner);
                    EXPECT_EQ(ready->snapshot().state, State::Ready);
                    ++callbacks;
                });
        });
    }
    for (auto &thread : producers) {
        thread.join();
    }
    ASSERT_TRUE(gate.started());
    ASSERT_NE(handles[0], nullptr);
    for (const auto &handle : handles) {
        EXPECT_EQ(handle, handles[0]);
    }
    manager.pump(); // Must not wait for a decoder that is still blocked.
    EXPECT_EQ(gpu.uploads, 0);
    EXPECT_EQ(callbacks, 0);
    EXPECT_EQ(manager.pendingCount(), count);
    gate.open();
    auto synchronous = manager.loadSync("images", "one");
    EXPECT_EQ(synchronous, handles[0]);
    EXPECT_EQ(synchronous->snapshot().state, State::Ready);
    EXPECT_EQ(callbacks, 0);
    EXPECT_EQ(gpu.decodes, 1);
    EXPECT_EQ(gpu.uploads, 1);
    manager.drain();
    EXPECT_EQ(callbacks, static_cast<int>(count));
    manager.pump();
    EXPECT_EQ(callbacks, static_cast<int>(count));
    manager.shutdown();
}

TEST(TextureManager, WorkerCacheHitsDispatchOnOwnerAndRespectCompletionBudget) {
    FakeGPU gpu;
    CTextureManager manager(gpu.backend());
    manager.loadSync("images", "one");
    std::atomic<int> callbacks{0};
    std::thread producer([&] {
        for (int i = 0; i < 60; ++i) {
            EXPECT_NE(manager.requestAsync(
                          "images", "one",
                          [&](const auto &) {
                              EXPECT_EQ(std::this_thread::get_id(), gpu.owner);
                              ++callbacks;
                          }),
                      nullptr);
        }
        EXPECT_EQ(callbacks, 0);
    });
    producer.join();
    manager.pump();
    EXPECT_EQ(callbacks, 50);
    EXPECT_EQ(manager.pendingCount(), 10U);
    manager.pump();
    EXPECT_EQ(callbacks, 60);
    EXPECT_EQ(manager.pendingCount(), 0U);
    EXPECT_EQ(gpu.decodes, 1);
    EXPECT_EQ(gpu.uploads, 1);
    manager.shutdown();
}

TEST(TextureManager, CallbackCanEnqueueAndDrainWithoutWaitingForItself) {
    FakeGPU gpu;
    CTextureManager manager(gpu.backend());
    int callbacks = 0;
    manager.requestAsync("images", "one", [&](const auto &) {
        manager.requestAsync("images", "two",
                             [&](const auto &) { ++callbacks; });
        manager.drain();
        ++callbacks;
        throw std::runtime_error("consumer failed");
    });
    manager.requestAsync("images", "one", [&](const auto &) { ++callbacks; });
    manager.drain();
    EXPECT_EQ(callbacks, 3);
    EXPECT_EQ(gpu.errors, 1);
    EXPECT_EQ(manager.pendingCount(), 0U);
    manager.shutdown();
}

TEST(TextureManager,
     SaturationRejectsAsyncButDoesNotPreventSynchronousLoading) {
    FakeGPU gpu;
    Gate gate;
    auto backend = gpu.backend();
    backend.decode = [&](const std::string &key) {
        ++gpu.decodes;
        if (key == "images/blocked") {
            gate.enter();
        }
        return pixel();
    };
    CTextureManager manager(std::move(backend), 2);
    OpenOnExit release{gate};
    int callbacks = 0;
    manager.requestAsync("images", "blocked",
                         [&](const auto &) { ++callbacks; });
    manager.requestAsync("images", "blocked",
                         [&](const auto &) { ++callbacks; });
    ASSERT_TRUE(gate.started());
    EXPECT_EQ(manager.requestAsync("images", "rejected"), nullptr);
    EXPECT_EQ(manager.find("images/rejected"), nullptr);
    auto direct = manager.loadSync("images", "direct");
    EXPECT_EQ(direct->snapshot().state, State::Ready);
    EXPECT_EQ(callbacks, 0);
    gate.open();
    manager.drain();
    EXPECT_EQ(callbacks, 2);
    EXPECT_NE(manager.requestAsync("images", "later"), nullptr);
    manager.drain();
    manager.shutdown();
}

TEST(TextureManager, FailuresAreTerminalAndCompletionIsDeliveredOnce) {
    FakeGPU gpu;
    auto backend = gpu.backend();
    backend.decode = [&](const std::string &key) {
        ++gpu.decodes;
        if (key == "images/broken") {
            throw std::runtime_error("bad image");
        }
        return pixel();
    };
    backend.upload = [&](CTextureImage &) -> unsigned int {
        ++gpu.uploads;
        throw std::runtime_error("upload failed");
    };
    CTextureManager manager(std::move(backend));
    int callbacks = 0;
    for (const auto *name : {"broken", "upload", ""}) {
        manager.requestAsync("images", name, [&](const auto &resource) {
            const auto result = resource->snapshot();
            EXPECT_EQ(result.state, State::Failed);
            EXPECT_EQ(result.id, 0U);
            EXPECT_FALSE(result.error.empty());
            ++callbacks;
        });
    }
    manager.drain();
    manager.pump();
    EXPECT_EQ(callbacks, 3);
    EXPECT_EQ(gpu.decodes, 2);
    EXPECT_EQ(gpu.uploads, 1);
    auto failed = manager.loadSync("images", "broken");
    EXPECT_EQ(failed->snapshot().state, State::Failed);
    EXPECT_EQ(gpu.decodes, 2); // An async failure retains its cache identity.
    manager.shutdown();
    EXPECT_EQ(gpu.destroys, 0);
}

TEST(TextureManager, SynchronousFailureCanBeRetriedAndZeroUploadIsFailure) {
    FakeGPU gpu;
    auto backend = gpu.backend();
    backend.upload = [&](CTextureImage &) {
        return ++gpu.uploads == 1 ? 0U : 17U;
    };
    CTextureManager manager(std::move(backend));
    auto first = manager.loadSync("images", "one");
    EXPECT_EQ(first->snapshot().state, State::Failed);
    EXPECT_FALSE(first->snapshot().error.empty());
    auto second = manager.loadSync("images", "one");
    EXPECT_EQ(first, second);
    EXPECT_EQ(second->snapshot().state, State::Ready);
    EXPECT_EQ(second->snapshot().id, 17U);
    manager.shutdown();
    EXPECT_EQ(gpu.destroys, 1);
}

TEST(TextureManager,
     PixelRetentionIsPerResourceAndCanBeRequestedWhileDecoding) {
    FakeGPU gpu;
    Gate gate;
    auto backend = gpu.backend();
    backend.decode = [&](const std::string &) {
        gate.enter();
        return pixel();
    };
    CTextureManager manager(std::move(backend));
    OpenOnExit release{gate};
    auto atlas = manager.requestAsync("images", "atlas");
    ASSERT_TRUE(gate.started());
    EXPECT_EQ(manager.requestAsync("images", "atlas", {}, true), atlas);
    gate.open();
    manager.drain();
    auto retained = atlas->snapshot().pixels;
    ASSERT_NE(retained, nullptr);
    EXPECT_EQ(retained->DataBytes, 4U);
    auto ordinary = manager.loadSync("images", "ordinary");
    EXPECT_EQ(ordinary->snapshot().pixels, nullptr);
    auto late = manager.loadSync("images", "ordinary", true);
    EXPECT_EQ(late, ordinary);
    EXPECT_EQ(late->snapshot().pixels, nullptr); // Caller may reread CPU data.
    EXPECT_EQ(gpu.uploads, 2);
    manager.shutdown();
    EXPECT_NE(retained->Data, nullptr); // An owned CPU view survives shutdown.
}

TEST(TextureManager, SynchronousUseRemovesSilentPrefetchButNotCallbacks) {
    FakeGPU gpu;
    CTextureManager manager(gpu.backend());
    auto silent = manager.requestAsync("images", "one");
    EXPECT_EQ(manager.requestAsync("images", "one"), silent);
    int callbacks = 0;
    manager.requestAsync("images", "one", [&](const auto &) { ++callbacks; });
    EXPECT_EQ(manager.pendingCount(), 2U);
    EXPECT_EQ(manager.loadSync("images", "one"), silent);
    EXPECT_EQ(manager.pendingCount(), 1U);
    EXPECT_EQ(callbacks, 0);
    manager.drain();
    EXPECT_EQ(callbacks, 1);
    manager.shutdown();
}

TEST(TextureManager, WorkerDrainCannotLoseCompletionNotification) {
    FakeGPU gpu;
    CTextureManager manager(gpu.backend());
    for (int iteration = 0; iteration < 40; ++iteration) {
        auto resource =
            manager.requestAsync("images", std::to_string(iteration));
        resource->waitForDecode();
        std::thread waiter([&] { manager.drain(); });
        // Completion may precede the waiter's entry or follow its wait.
        manager.pump();
        waiter.join();
        EXPECT_EQ(manager.pendingCount(), 0U);
    }
    manager.shutdown();
}

TEST(TextureManager, ShutdownJoinsDecodersCancelsCallbacksAndWakesWaiters) {
    FakeGPU gpu;
    Gate gate;
    auto backend = gpu.backend();
    backend.decode = [&](const std::string &key) {
        if (key == "images/blocked") {
            gate.enter();
        }
        return pixel();
    };
    CTextureManager manager(std::move(backend));
    OpenOnExit release{gate};
    manager.loadSync("images", "uploaded");
    int callbacks = 0;
    auto pending = manager.requestAsync("images", "blocked",
                                        [&](const auto &) { ++callbacks; });
    ASSERT_TRUE(gate.started());
    std::atomic<bool> waiterDone{false};
    std::thread waiter([&] {
        manager.drain();
        waiterDone = true;
    });
    std::thread releaser([&] {
        while (!manager.closed()) {
            std::this_thread::yield();
        }
        gate.open();
    });
    manager.shutdown();
    releaser.join();
    waiter.join();
    EXPECT_TRUE(waiterDone);
    EXPECT_EQ(callbacks, 0);
    EXPECT_EQ(gpu.uploads, 1);
    EXPECT_EQ(gpu.destroys, 1);
    EXPECT_EQ(pending->snapshot().state, State::Cancelled);
    EXPECT_EQ(manager.pendingCount(), 0U);
    EXPECT_EQ(manager.requestAsync("images", "late"), nullptr);
    EXPECT_EQ(manager.loadSync("images", "late"), nullptr);
    manager.shutdown();
}

TEST(TextureManager, QueriesRemainCoherentWhileOwnerPublishes) {
    FakeGPU gpu;
    CTextureManager manager(gpu.backend());
    auto resource = manager.requestAsync("images", "one");
    std::atomic<bool> finished{false};
    std::thread reader([&] {
        do {
            const auto state = resource->snapshot();
            if (state.state == State::Ready) {
                EXPECT_NE(state.id, 0U);
                EXPECT_EQ(state.width, 1);
                EXPECT_EQ(state.height, 1);
            } else {
                EXPECT_EQ(state.id, 0U);
            }
        } while (!finished);
    });
    manager.drain();
    manager.shutdown();
    finished = true;
    reader.join();
}

TEST(TextureManager, PublicationAndGpuReleaseRejectNonOwnerThreads) {
    FakeGPU gpu;
    CTextureManager manager(gpu.backend());
    std::thread worker([&] {
        EXPECT_THROW(manager.loadSync("images", "one"), std::logic_error);
        EXPECT_THROW(manager.pump(), std::logic_error);
        EXPECT_THROW(manager.shutdown(), std::logic_error);
        EXPECT_NE(manager.requestAsync("images", "one"), nullptr);
    });
    worker.join();
    manager.drain();
    manager.shutdown();
}

TEST(TextureManager, HandlesOutliveManagerWithoutKeepingJobsOrGpuIds) {
    FakeGPU gpu;
    CTextureManager::Handle handle;
    {
        CTextureManager manager(gpu.backend());
        handle = manager.requestAsync("images", "one");
        // Destructor joins CPU work but does not upload or call user code.
    }
    EXPECT_EQ(handle->snapshot().state, State::Cancelled);
    EXPECT_EQ(handle->snapshot().id, 0U);
    EXPECT_EQ(gpu.uploads, 0);
}
} // namespace
