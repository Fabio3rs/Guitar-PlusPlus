#include "CLog.h"
#include "GPPGame.h"
#include <filesystem>

void GPPGame::initializeModels() {
    if (modelManager && !modelManager->closed()) {
        return;
    }
    basicModels.clear();
    modelManager = std::make_unique<CModelManager>(
        readModelData, [this](const CModelData &data, bool wait) {
            if (textureManager->closed()) {
                throw std::runtime_error("Model texture loader was shut down");
            }
            bool ready = true;
            for (const auto &entry : data.materials) {
                const auto &material = entry.second;
                if (material.values.diffuseMap.empty()) {
                    continue;
                }
                const auto &file = material.values.diffuseMap;
                if (wait) {
                    loadTexture(material.directory, file);
                } else {
                    if (!loadTextureSingleAsync(loadTextureBatch(
                            material.directory, file, nullptr, nullptr))) {
                        ready = false;
                        continue;
                    }
                }
                const auto status =
                    getTextureInfo(material.directory + "/" + file);
                if (status.state == CTextureResource::State::Cancelled) {
                    throw std::runtime_error(
                        "Model texture loader was shut down");
                }
                if (status.state != CTextureResource::State::Ready &&
                    status.state != CTextureResource::State::Failed) {
                    ready = false;
                }
            }
            return ready;
        });
}

CModelManager &GPPGame::modelLoader() { return *modelManager; }

CModelManager::Handle GPPGame::requestModel(const std::string &path,
                                            const std::string &file,
                                            GPPOBJ &target) {
    // Taking a publication target is an owner operation, even though the CPU
    // manager itself accepts requests from other threads.
    if (!modelManager->isOwnerThread()) {
        return {};
    }
    auto handle =
        modelManager->requestAsync(target.requestDescription(path, file));
    if (handle && handle->snapshot().state == CModelManager::State::Decoding) {
        modelManager->setPublisher(handle, target.publisher());
    }
    return handle;
}

bool GPPGame::loadModelSync(const std::string &path, const std::string &file,
                            GPPOBJ &target) {
    if (!modelManager->isOwnerThread() || modelManager->closed()) {
        return false;
    }
    const auto handle = modelManager->loadSync(
        target.requestDescription(path, file), target.publisher());
    if (!handle) {
        return false;
    }
    const auto result = handle->snapshot();
    if (result.state != CModelManager::State::Ready) {
        CLog::log() << "Model loading failed: " + result.error;
        return false;
    }
    return true;
}

bool GPPGame::loadSingleModelAsync(loadModelBatch batch) {
    return batch.obj &&
           static_cast<bool>(requestModel(batch.path, batch.model, *batch.obj));
}

bool GPPGame::loadModelBatchAsync(std::deque<loadModelBatch> &batch) {
    bool accepted = true;
    for (auto &entry : batch) {
        entry.handle =
            entry.obj
                ? GuitarPP().requestModel(entry.path, entry.model, *entry.obj)
                : nullptr;
        accepted = static_cast<bool>(entry.handle) && accepted;
    }
    return accepted;
}

size_t GPPGame::getNumModelsToLoad() const {
    return modelManager ? modelManager->pendingCount() : 0;
}

bool GPPGame::basicModelsReady() const {
    if (basicModels.size() != 5) {
        return false;
    }
    for (const auto &handle : basicModels) {
        const auto result = handle->snapshot();
        if (result.state != CModelManager::State::Ready) {
            CLog::log() << "Required model not ready: " + result.error;
            return false;
        }
    }
    return true;
}

void GPPGame::shutdownModels() {
    if (modelManager) {
        modelManager->shutdown();
    }
    basicModels.clear();
    noteOBJ.unload();
    triggerBASEOBJ.unload();
    triggerOBJ.unload();
    pylmbarOBJ.unload();
    openNoteOBJ.unload();
    testobj.unload();
}
