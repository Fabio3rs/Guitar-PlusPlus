#pragma once
#include "CEngine.h"
#include "CModelManager.h"
#include <functional>
#include <memory>

class GPPOBJ {
    struct Impl;
    std::shared_ptr<Impl> impl; // Publishers retain only weak references.
    void prepareBuffers();

  public:
    // Called only during upload. The provider owns returned textures and must
    // outlive this model. An empty provider retains the game's texture manager.
    using TextureLoader = std::function<unsigned int(const std::string &)>;

    struct PartInfo {
        std::string object, group, material;
        size_t vertices = 0;
        size_t id = 0, triangles = 0;
        ObjModel::Vec3 minimum{}, maximum{};
    };

    struct TextureInfo {
        std::string path;
        bool loaded = false;
        int width = 0, height = 0;
    };

    struct Inspection {
        std::vector<PartInfo> parts;
        std::map<std::string, ObjModel::Material> materials;
        std::vector<std::string> warnings;
        bool aoAtlas = false;
        std::map<std::string, TextureInfo> textures;
    };

    // Texture sizes require the current GL context; default inspection is
    // CPU-only.
    Inspection inspect(bool textureSizes = false) const;

    // Optional owner-provided mask; IDs are part indices for this model
    // revision.
    using Visibility = std::vector<unsigned char>;
    struct Hit {
        int part = -1;
        ObjModel::Vec3 point{};
        double distance = 0;
    };
    // CPU-only, two-sided geometric picking (texture alpha is not sampled).
    Hit raycast(const ObjModel::Vec3 &origin, const ObjModel::Vec3 &direction,
                const Visibility *visible = nullptr) const;

    using MaterialBinder = std::function<bool(
        const ObjModel::Part &, const ObjModel::Material &, bool)>;

    bool keepModelDataLoaded = false;

    CModelRequest requestDescription(const std::string &path,
                                     const std::string &file,
                                     const std::string &object = "") const;
    // Takes CPU data by value; shared results copy, standalone loads move.
    // Preserves the legacy deferred-upload contract.
    void setModelData(CModelData data);
    // Owner-only. Returns false if this target was unloaded, superseded or
    // destroyed. A move transfers the target; move assignment retires the old
    // one.
    CModelManager::Publisher publisher();

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
    void drawMaterials(const MaterialBinder &bind, unsigned int texture = 0,
                       const Visibility *visible = nullptr);
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
