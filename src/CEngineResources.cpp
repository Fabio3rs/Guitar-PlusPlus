#include "CEngineResources.h"
#include "CEngine.h"
#include "CLog.h"
#include <stdexcept>

CTextureManager::Backend makeEngineTextureBackend() {
    auto *engine = &CEngine::engine();
    return {[engine](const std::string &file) {
                CTextureImage image;
                if (!engine->readImage(file.c_str(), &image, 0)) {
                    throw std::runtime_error("Cannot decode texture: " + file);
                }
                return image;
            },
            [engine](CTextureImage &image) {
                if (!engine->hasRenderContext()) {
                    throw std::runtime_error(
                        "Texture upload requires a current render context");
                }
                return engine->uploadTextureToOGL(&image);
            },
            [engine](unsigned id) { engine->destroyTexture(id); },
            [](const std::string &error) { CLog::log() << error; }};
}
