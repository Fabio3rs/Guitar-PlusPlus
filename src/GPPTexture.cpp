#include "CEngineResources.h"
#include "CLog.h"
#include "GPPGame.h"
#include <stdexcept>
#include <utility>

namespace {
unsigned int legacyId(const CTextureManager::Handle &resource) {
    if (!resource) {
        return 0;
    }
    const auto status = resource->snapshot();
    if (status.state == CTextureResource::State::Failed &&
        status.decodedAsync) {
        return ~0U;
    }
    return status.id;
}

} // namespace

void GPPGame::initializeTextures() {
    if (textureManager && !textureManager->closed()) {
        return;
    }
    textureViews.clear();
    textureManager =
        std::make_unique<CTextureManager>(makeEngineTextureBackend());
}

void GPPGame::shutdownTextures() {
    if (textureManager) {
        textureManager->shutdown();
    }
}

GPPGame::gppTexture &
GPPGame::textureView(const CTextureManager::Handle &resource,
                     CLuaH::luaScript *script) {
    if (!resource) {
        return unavailableTexture;
    }
    std::lock_guard<std::mutex> lock(textureViewsMutex);
    auto &view =
        textureViews.try_emplace(resource->key, *textureManager, resource)
            .first->second;
    if (script != nullptr) {
        view.associatedToScript[script] = true;
    }
    return view;
}

const GPPGame::gppTexture &GPPGame::loadTexture(const std::string &path,
                                                const std::string &texture,
                                                CLuaH::luaScript *script,
                                                bool keepPixels) {
    auto resource =
        textureManager->isOwnerThread()
            ? textureManager->loadSync(path, texture, keepPixels)
            : textureManager->requestAsync(path, texture, {}, keepPixels);
    return textureView(resource, script);
}

bool GPPGame::loadTextureSingleAsync(const loadTextureBatch &data) {
    // A cache-hit callback may destroy the caller's original request.
    // Copy everything needed after dispatch before entering the manager.
    auto *script = data.luaScript;
    CTextureManager::Completion completion;
    if (data.targetFun) {
        // Own the batch; never pass the address of a queue slot to user code.
        completion = [this, batch = data](
                         const CTextureManager::Handle &resource) mutable {
            batch.text = &textureView(resource, batch.luaScript);
            batch.targetFun(&batch);
        };
    }
    auto resource = textureManager->requestAsync(data.path, data.texture,
                                                 std::move(completion));
    if (resource) {
        textureView(resource, script);
    }
    return static_cast<bool>(resource);
}

bool GPPGame::loadTextureBatchAsync(std::deque<loadTextureBatch> &batch) {
    // Historical blocking API despite its name. Off-owner requests cannot
    // uphold its GL-ready-on-return contract and are rejected explicitly.
    if (!textureManager->isOwnerThread() || textureManager->closed()) {
        return false;
    }
    for (const auto &item : batch) {
        // Start CPU work in parallel where there is capacity. loadSync below
        // also handles resources rejected by the asynchronous queue limit.
        textureManager->requestAsync(item.path, item.texture);
    }
    for (auto &item : batch) {
        auto resource = textureManager->loadSync(item.path, item.texture);
        item.text = &textureView(resource, item.luaScript);
        if (item.targetFun) {
            try {
                item.targetFun(&item);
            } catch (...) {
                CLog::log() << "Texture batch completion callback failed";
            }
        }
    }
    return true;
}

void GPPGame::forceTexturesToLoad() { textureManager->drain(); }

int GPPGame::getNumTexturesToLoad() {
    return static_cast<int>(textureManager->pendingCount());
}

unsigned int GPPGame::getTextureId(const std::string &name) const noexcept {
    return legacyId(textureManager->find(name));
}

CTextureResource::Snapshot
GPPGame::getTextureInfo(const std::string &key) const {
    auto resource = textureManager->find(key);
    return resource ? resource->snapshot() : CTextureResource::Snapshot{};
}

GPPGame::gppTexture::gppTexture(CTextureManager &owner,
                                CTextureManager::Handle handle)
    : manager(&owner), resource(std::move(handle)), textPath(resource->path),
      textName(resource->name) {}

GPPGame::gppTexture::gppTexture(const std::string &path,
                                const std::string &name, bool async)
    : manager(GuitarPP().textureManager.get()), textPath(path), textName(name),
      asynchronous(async) {}

GPPGame::gppTexture::gppTexture(gppTexture &&other) noexcept {
    *this = std::move(other);
}

GPPGame::gppTexture &
GPPGame::gppTexture::operator=(gppTexture &&other) noexcept {
    if (this != &other) {
        if (other.resource) {
            other.resource->waitForDecode();
        }
        manager = other.manager;
        resource = std::move(other.resource);
        textPath = std::move(other.textPath);
        textName = std::move(other.textName);
        asynchronous = other.asynchronous;
        associatedToScript = std::move(other.associatedToScript);
        other.manager = nullptr;
    }
    return *this;
}

bool GPPGame::gppTexture::isAsyncRunning() const {
    return resource && resource->isDecoding();
}

unsigned int GPPGame::gppTexture::getTextId() const {
    return legacyId(resource);
}

unsigned int GPPGame::gppTexture::getTextIdUpdateAsync() {
    if (manager != nullptr) {
        resource = manager->loadSync(textPath, textName);
    }
    return getTextId();
}

int GPPGame::gppTexture::getImgWidth() const {
    return resource ? resource->snapshot().width : 0;
}

int GPPGame::gppTexture::getImgHeight() const {
    return resource ? resource->snapshot().height : 0;
}

std::shared_ptr<const CTextureImage> GPPGame::gppTexture::getImageData() const {
    return resource ? resource->snapshot().pixels : nullptr;
}

void GPPGame::gppTexture::load() {
    if (manager != nullptr) {
        resource = manager->loadSync(textPath, textName);
    }
}

void GPPGame::gppTexture::tloadAsync(const std::string &path,
                                     const std::string &name) {
    if ((manager != nullptr) && asynchronous && (getTextId() == 0u)) {
        textPath = path;
        textName = name;
        resource = manager->requestAsync(path, name);
    }
}
