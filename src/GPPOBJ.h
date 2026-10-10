#pragma once
#include "CEngine.h"
#include "ObjModel.h"
#include <functional>
#include <memory>

class GPPOBJ {
    struct Impl;
    std::unique_ptr<Impl> impl;
    void prepareBuffers();

  public:
    // Called only during upload. The provider owns returned textures and must
    // outlive this model. An empty provider retains the game's texture manager.
    using TextureLoader = std::function<unsigned int(const std::string &)>;

    struct PartInfo {
        std::string object, group, material;
        size_t vertices = 0;
    };

    struct Inspection {
        std::vector<PartInfo> parts;
        std::map<std::string, ObjModel::Material> materials;
        std::vector<std::string> warnings;
        bool aoAtlas = false;
    };

    Inspection inspect() const;
    using MaterialBinder = std::function<bool(
        const ObjModel::Part &, const ObjModel::Material &, bool)>;
    bool keepModelDataLoaded = false;
    bool loadInternalObj(const std::string &path, const std::string &file,
                         const std::string &objName = "",
                         bool loadMtlLib = false);
    // CPU-only explicit MTL selection for a model not yet loaded.
    void setMaterialLibrary(const std::string &file);
    void loadTextureList(const std::string &path, const std::string &file);
    bool load(const std::string &path, const std::string &file);
    void draw(unsigned int texture, bool autoBindZeroVBO = true);
    void onlyDraw(bool autoBindZeroVBO = true) const;
    // All GL work (including lazy upload) happens here, on the rendering
    // thread.
    void prepareForDraw();
    void drawMaterials(const MaterialBinder &bind, unsigned int texture = 0);
    std::pair<ObjModel::Vec3, ObjModel::Vec3> bounds() const;
    size_t revision() const;
    bool hasAO() const;
    gppVec3f boxTestForMtl(const std::string &mtl);
    void unload();
    GPPOBJ();
    explicit GPPOBJ(TextureLoader loader);
    ~GPPOBJ();
    GPPOBJ(GPPOBJ &&) noexcept;
    GPPOBJ &operator=(GPPOBJ &&) noexcept;
    GPPOBJ(const GPPOBJ &) = delete;
    GPPOBJ &operator=(const GPPOBJ &) = delete;
};
