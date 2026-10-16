#include "CModelManager.h"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace {
using State = CModelManager::State;
struct Gate {
    std::promise<void> opened;
    std::shared_future<void> ready = opened.get_future().share();
    bool released = false;
    void release() {
        if (!released) {
            released = true;
            opened.set_value();
        }
    }
    ~Gate() { release(); }
};
// Declare this after the manager so failing assertions unblock before joining.
struct ReleaseGate {
    Gate &gate;
    ~ReleaseGate() { gate.release(); }
};

struct ModelScratch {
    std::filesystem::path directory;
    ModelScratch() {
        // create_directory reserves a distinct path even across sanitizer runs.
        for (size_t suffix = 0;; ++suffix) {
            auto candidate = std::filesystem::temp_directory_path() /
                             ("gpp-model-test-" + std::to_string(suffix));
            if (std::filesystem::create_directory(candidate)) {
                directory = std::move(candidate);
                break;
            }
        }
    }
    ~ModelScratch() { std::filesystem::remove_all(directory); }
};

TEST(ModelData, ManagerPreservesFilesystemMeaningOfSymlinkParentPaths) {
    ModelScratch scratch;
    const auto &dir = scratch.directory;
    std::filesystem::create_directories(dir / "actual" / "nested");
    std::error_code error;
    std::filesystem::create_directory_symlink(dir / "actual" / "nested",
                                              dir / "link", error);
    if (error) {
        GTEST_SKIP() << "Cannot create directory symlink: " << error.message();
    }
    std::ofstream(dir / "model.obj") << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    std::ofstream(dir / "actual" / "model.obj")
        << "v 0 0 0\nv 2 0 0\nv 0 2 0\nf 1 2 3\n";
    std::ofstream(dir / "material.mtl") << "newmtl surface\nKd 0 0 1\nmap_Kd texture.tga\n";
    std::ofstream(dir / "actual" / "material.mtl")
        << "newmtl surface\nKd 1 0 0\nmap_Kd texture.tga\n";
    std::ofstream(dir / "texture.tga") << "decoy";
    std::ofstream(dir / "actual" / "texture.tga") << "actual";
    CModelManager manager;
    auto handle =
        manager.loadSync({(dir / "link" / ".." / "model.obj").string(), "",
                          (dir / "link" / ".." / "material.mtl").string()});
    auto result = handle->snapshot();
    ASSERT_EQ(result.state, State::ReadyForUpload) << result.error;
    ASSERT_NE(result.data, nullptr);
    // Win32 resolves .. before following the directory symlink; POSIX
    // follows the symlink first. Match the filesystem, not POSIX semantics.
    const bool followsLinkFirst = std::filesystem::equivalent(
        dir / "link" / ".." / "model.obj", dir / "actual" / "model.obj");
    EXPECT_FLOAT_EQ(result.data->model.maximum[0], followsLinkFirst ? 2.f : 1.f);
    const auto &material = result.data->materials.at("surface");
    EXPECT_TRUE(std::filesystem::equivalent(
        std::filesystem::path(material.directory) / material.values.diffuseMap,
        (followsLinkFirst ? dir / "actual" : dir) / "texture.tga"));
    EXPECT_EQ(result.data->materials.at("surface").values.diffuse,
              (followsLinkFirst ? ObjModel::Vec3{1, 0, 0}
                                : ObjModel::Vec3{0, 0, 1}));
}

TEST(ModelManager, PublicationMayDestroyTheCallersHandle) {
    CModelManager manager([](const CModelRequest &) { return CModelData{}; });
    auto caller = std::make_unique<CModelManager::Handle>();
    *caller =
        manager.requestAsync({"owned.obj", "", ""}, [&](const CModelData &) {
            caller.reset();
            return true;
        });
    auto observer = *caller;
    EXPECT_TRUE(manager.finish(*caller));
    EXPECT_EQ(caller, nullptr);
    EXPECT_EQ(observer->snapshot().state, State::Ready);
    EXPECT_EQ(manager.pendingCount(), 0u);
}

TEST(ModelManager, DependenciesMayReplaceTheCallersHandle) {
    CModelManager::Handle caller, other;
    CModelManager manager([](const CModelRequest &) { return CModelData{}; },
                          [&](const CModelData &, bool) {
                              caller = other;
                              return true;
                          });
    caller = manager.requestAsync({"first.obj", "", ""},
                                  [](const CModelData &) { return true; });
    other = manager.requestAsync({"other.obj", "", ""});
    auto observer = caller;
    EXPECT_TRUE(manager.finish(caller));
    EXPECT_EQ(observer->snapshot().state, State::Ready);
    EXPECT_EQ(other->snapshot().state, State::Decoding);
    EXPECT_EQ(manager.pendingCount(), 1u);
    manager.finish(other);
}

TEST(ModelManager, SyncPublishesBeforeReturnAndDoesNotPumpOtherRequests) {
    int decoded = 0, first = 0, second = 0;
    CModelManager manager([&](const CModelRequest &) {
        ++decoded;
        return CModelData{};
    });
    auto async =
        manager.requestAsync({"same.obj", "", ""}, [&](const CModelData &) {
            ++first;
            return true;
        });
    auto sync = manager.loadSync({"same.obj", "", ""}, [&](const CModelData &) {
        ++second;
        return true;
    });
    ASSERT_NE(sync, nullptr);
    EXPECT_EQ(sync->snapshot().state, State::Ready);
    EXPECT_EQ(decoded, 1);
    EXPECT_EQ(first, 0);
    EXPECT_EQ(second, 1);
    EXPECT_EQ(manager.pendingCount(), 1u);
    manager.finish(async);
    EXPECT_EQ(first, 1);
    EXPECT_EQ(manager.pendingCount(), 0u);
}

TEST(ModelManager,
     ConcurrentRequestsShareCpuWorkButPublishIndependentlyOnOwner) {
    Gate gate;
    std::atomic<int> decoded{0};
    const auto owner = std::this_thread::get_id();
    CModelManager manager([&](const CModelRequest &) {
        ++decoded;
        gate.ready.wait();
        return CModelData{};
    });
    ReleaseGate release{gate};
    std::vector<CModelManager::Handle> handles(12);
    std::vector<std::thread> callers;
    int published = 0;
    for (size_t i = 0; i < handles.size(); ++i) {
        callers.emplace_back([&, i] {
            handles[i] = manager.requestAsync(
                {"same.obj", "", ""}, [&](const CModelData &) {
                    EXPECT_EQ(std::this_thread::get_id(), owner);
                    ++published;
                    return true;
                });
        });
    }
    for (auto &caller : callers) {
        caller.join();
    }
    EXPECT_EQ(published, 0);
    gate.release();
    for (const auto &handle : handles) {
        ASSERT_NE(handle, nullptr);
        EXPECT_TRUE(manager.finish(handle));
    }
    EXPECT_EQ(decoded, 1);
    EXPECT_EQ(published, 12);
}

TEST(ModelManager, CpuOnlySyncKeepsDeferredUploadContract) {
    const auto owner = std::this_thread::get_id();
    CModelManager manager([&](const CModelRequest &) {
        EXPECT_EQ(std::this_thread::get_id(), owner);
        return CModelData{};
    });
    auto handle = manager.loadSync({"cpu.obj", "", ""});
    EXPECT_EQ(handle->snapshot().state, State::ReadyForUpload);
    EXPECT_NE(handle->snapshot().data, nullptr);
    EXPECT_EQ(manager.pendingCount(), 0u);
}

TEST(ModelManager, DecoderAndPublisherFailuresAreReportedAndRetryable) {
    std::atomic<int> attempts{0};
    CModelManager manager([&](const CModelRequest &) {
        if (++attempts == 1) {
            throw std::runtime_error("decode failed");
        }
        return CModelData{};
    });
    auto failed = manager.loadSync({"retry.obj", "", ""});
    EXPECT_EQ(failed->snapshot().state, State::Failed);
    EXPECT_EQ(failed->snapshot().error, "decode failed");
    auto upload =
        manager.loadSync({"retry.obj", "", ""}, [](const CModelData &) -> bool {
            throw std::runtime_error("upload failed");
        });
    EXPECT_EQ(upload->snapshot().state, State::Failed);
    EXPECT_EQ(upload->snapshot().error, "upload failed");
    auto retry = manager.loadSync({"retry.obj", "", ""},
                                  [](const CModelData &) { return true; });
    EXPECT_EQ(retry->snapshot().state, State::Ready);
}

TEST(ModelManager, DependenciesDeferAsyncAndCompleteSynchronously) {
    bool ready = false;
    int published = 0;
    CModelManager manager([](const CModelRequest &) { return CModelData{}; },
                          [&](const CModelData &, bool wait) {
                              ready = ready || wait;
                              return ready;
                          });
    auto handle =
        manager.requestAsync({"material.obj", "", ""}, [&](const CModelData &) {
            ++published;
            return true;
        });
    // Join the shared decoder through a CPU-only request without pumping
    // upload.
    manager.loadSync({"material.obj", "", ""});
    EXPECT_FALSE(manager.finish(handle, false));
    EXPECT_EQ(handle->snapshot().state, State::WaitingForTextures);
    EXPECT_EQ(published, 0);
    EXPECT_TRUE(manager.finish(handle, true));
    EXPECT_EQ(handle->snapshot().state, State::Ready);
    EXPECT_EQ(published, 1);
}

TEST(ModelManager, CapacityCancellationAndShutdownDoNotPublishAbandonedWork) {
    Gate gate;
    CModelManager manager(
        [&](const CModelRequest &) {
            gate.ready.wait();
            return CModelData{};
        },
        {}, 1);
    ReleaseGate release{gate};
    int publications = 0;
    auto handle =
        manager.requestAsync({"one.obj", "", ""}, [&](const CModelData &) {
            ++publications;
            return true;
        });
    ASSERT_NE(handle, nullptr);
    EXPECT_EQ(manager.requestAsync({"two.obj", "", ""}), nullptr);
    manager.cancel(handle);
    EXPECT_EQ(handle->snapshot().state, State::Cancelled);
    EXPECT_EQ(manager.pendingCount(), 0u);
    // Cancelled CPU work still occupies capacity until it actually completes.
    EXPECT_EQ(manager.requestAsync({"two.obj", "", ""}), nullptr);
    gate.release();
    manager.shutdown();
    EXPECT_EQ(publications, 0);
    EXPECT_EQ(manager.requestAsync({"three.obj", "", ""}), nullptr);
    EXPECT_TRUE(manager.closed());
}

TEST(ModelManager,
     ShutdownWaitsForRunningCpuWithoutPublishingOrHoldingQueueLock) {
    Gate gate;
    std::promise<void> started;
    auto running = started.get_future();
    CModelManager manager([&](const CModelRequest &) {
        started.set_value();
        gate.ready.wait();
        return CModelData{};
    });
    ReleaseGate release{gate};
    int publications = 0;
    auto handle =
        manager.requestAsync({"running.obj", "", ""}, [&](const CModelData &) {
            ++publications;
            return true;
        });
    running.wait();
    std::thread unblock([&] {
        while (!manager.closed()) {
            std::this_thread::yield();
        }
        gate.release();
    });
    manager.shutdown();
    unblock.join();
    EXPECT_EQ(publications, 0);
    EXPECT_EQ(handle->snapshot().state, State::Cancelled);
}

TEST(ModelManager, SynchronousRequestsBypassAsyncCapacity) {
    CModelManager manager([](const CModelRequest &) { return CModelData{}; },
                          {}, 1);
    auto pending = manager.requestAsync({"one.obj", "", ""});
    ASSERT_NE(pending, nullptr);
    EXPECT_EQ(manager.requestAsync({"two.obj", "", ""}), nullptr);
    auto sync = manager.loadSync({"two.obj", "", ""});
    ASSERT_NE(sync, nullptr);
    EXPECT_EQ(sync->snapshot().state, State::ReadyForUpload);
    EXPECT_EQ(manager.pendingCount(), 1u);
    manager.finish(pending);
}

TEST(ModelManager, PendingHandlesSurviveShutdownAndRejectForeignOwners) {
    CModelManager::Handle handle;
    {
        CModelManager manager(
            [](const CModelRequest &) { return CModelData{}; });
        handle = manager.requestAsync({"pending.obj", "", ""});
        std::thread wrong([&] {
            EXPECT_THROW(manager.finish(handle), std::logic_error);
            EXPECT_THROW(manager.loadSync({"sync.obj", "", ""}),
                         std::logic_error);
        });
        wrong.join();
        CModelManager other;
        EXPECT_THROW(other.finish(handle), std::invalid_argument);
    }
    EXPECT_EQ(handle->snapshot().state, State::Cancelled);
}

TEST(ModelManager, PublishersMayEnqueueAndReenterPumpWithoutDoublePublication) {
    int publications = 0;
    CModelManager manager([](const CModelRequest &) { return CModelData{}; });
    CModelManager::Handle second;
    auto first =
        manager.requestAsync({"first.obj", "", ""}, [&](const CModelData &) {
            ++publications;
            second = manager.requestAsync({"second.obj", "", ""});
            manager.pump();
            return true;
        });
    manager.finish(first);
    ASSERT_NE(second, nullptr);
    manager.finish(second);
    EXPECT_EQ(publications, 1);
    EXPECT_EQ(manager.pendingCount(), 0u);
}

TEST(ModelManager, ShutdownDuringDependenciesPreventsGpuPublication) {
    CModelManager *active = nullptr;
    int published = 0;
    CModelManager manager([](const CModelRequest &) { return CModelData{}; },
                          [&](const CModelData &, bool) {
                              active->shutdown();
                              return true;
                          });
    active = &manager;
    auto handle =
        manager.loadSync({"shutdown.obj", "", ""}, [&](const CModelData &) {
            ++published;
            return true;
        });
    EXPECT_EQ(handle->snapshot().state, State::Cancelled);
    EXPECT_EQ(published, 0);
}

TEST(ModelManager, PumpHonorsCompletionBudgetAndPublishesCoherentSnapshots) {
    CModelManager manager([](const CModelRequest &) { return CModelData{}; });
    std::vector<CModelManager::Handle> handles;
    for (int i = 0; i < 4; ++i) {
        handles.push_back(manager.requestAsync(
            {"shared.obj", "", ""}, [](const CModelData &) { return true; }));
    }
    manager.loadSync({"shared.obj", "", ""});
    std::atomic<bool> done{false};
    std::thread reader([&] {
        while (!done) {
            for (const auto &handle : handles) {
                const auto status = handle->snapshot();
                if (status.state == State::Ready) {
                    EXPECT_TRUE(status.error.empty());
                    EXPECT_EQ(status.data, nullptr);
                }
            }
        }
    });
    manager.pump(1);
    EXPECT_EQ(manager.pendingCount(), 3u);
    manager.pump(3);
    EXPECT_EQ(manager.pendingCount(), 0u);
    done = true;
    reader.join();
}

TEST(ModelManager, MaterialLibraryAndObjectFilterBelongToDecodeIdentity) {
    std::atomic<int> decoded{0};
    CModelManager manager([&](const CModelRequest &) {
        ++decoded;
        return CModelData{};
    });
    auto a = manager.requestAsync({"scene.obj", "one", "a.mtl"});
    auto b = manager.requestAsync({"scene.obj", "two", "a.mtl"});
    auto c = manager.requestAsync({"scene.obj", "one", "b.mtl"});
    manager.finish(a);
    manager.finish(b);
    manager.finish(c);
    EXPECT_EQ(decoded, 3);
}

TEST(ModelData, ReadsObjExplicitMaterialsAndOptionalAoWithoutGame) {
    ModelScratch scratch;
    const auto &dir = scratch.directory;
    std::filesystem::create_directories(dir / "textures");
    std::ofstream(dir / "scene.obj")
        << "mtllib absent.mtl\no one\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    std::ofstream(dir / "chosen.mtl")
        << "newmtl surface\nmap_Kd textures/wall.tga\n";
    auto data = readModelData(
        {(dir / "scene.obj").string(), "one", (dir / "chosen.mtl").string()});
    ASSERT_EQ(data.model.parts.size(), 1u);
    EXPECT_EQ(data.materials.at("surface").directory,
              (dir / "textures").string());
    EXPECT_EQ(data.materials.at("surface").values.diffuseMap, "wall.tga");
    EXPECT_TRUE(data.ao.pixels.empty());
    EXPECT_THROW(readModelData({(dir / "missing.obj").string(), "", ""}),
                 std::runtime_error);
    EXPECT_THROW(readModelData({(dir / "scene.obj").string(), "absent", ""}),
                 std::runtime_error);
    const auto plain = readModelData({(dir / "scene.obj").string(), "", ""});
    EXPECT_TRUE(plain.materials.empty());
    std::ofstream(dir / "scene.obj.ao") << "broken sidecar";
    auto brokenAo = readModelData({(dir / "scene.obj").string(), "", ""});
    EXPECT_FALSE(brokenAo.model.warnings.empty());
    EXPECT_EQ(brokenAo.model.parts.size(), 1u);
}
} // namespace
