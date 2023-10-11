#pragma once
#include <unordered_map>
#ifndef _GUITAR_PP_GPPOBJ_h_
#define _GUITAR_PP_GPPOBJ_h_
#include "CEngine.h"
#include "CLuaH.hpp"
#include <array>
#include <atomic>
#include <future>
#include <string>
#include <vector>

class GPPOBJ {
    std::vector<int8_t> data;
    CEngine::staticDrawBuffer vbodata;

    bool mtlLoaded, modelLoaded;

    std::mutex mtx;

    struct loadedTextures {
        std::string path, name;
    };

    std::map<unsigned int, loadedTextures> loadedTextures;

    struct modelPart {
        CEngine::staticDrawBuffer vbodata;
        std::vector<int8_t> data;
        unsigned int textureID;
        std::string mtl;

        inline modelPart() { textureID = 0; }

        inline ~modelPart() { vbodata.destroy(); }
    };

    struct mtl {
        std::string textureName;
        unsigned int textureID;

        inline mtl() { textureID = 0; }
    };

    std::unordered_map<std::string, mtl> mtlLib;

    void loadMtlLibData(const std::string &path, const std::string &file);

    struct modelPartIndex {
        std::string name;
        std::basic_string<unsigned int> vertexIndices;
        std::basic_string<unsigned int> uvIndices;
        std::basic_string<unsigned int> normalIndices;
    };

    struct modelData {
        std::vector<gppVec3f> vertices;
        std::vector<gppVec2f> uvs;
        std::vector<gppVec3f> normals;

        inline void clear() {
            vertices.clear();
            uvs.clear();
            normals.clear();
        }
    };

    std::vector<modelPart> multiData;

    std::string lastPath;

  public:
    bool loadInternalObj(const std::string &path, const std::string &file,
                         const std::string &objName = "",
                         bool loadMtlLib = false);
    bool loadInternalObjthr(const std::string &path, const std::string &file,
                            const std::string &objName = "",
                            bool loadMtlLib = false);

    bool keepModelDataLoaded;

    void draw(unsigned int texture, bool autoBindZeroVBO = true);
    void onlyDraw(bool autoBindZeroVBO = true) const;
    void loadTextureList(const std::string &path, const std::string &file);
    void load(const std::string &path, const std::string &file);

    gppVec3f boxTestForMtl(const std::string &mtl);
    // void reload(const std::string &path = "");
    void unload();

    GPPOBJ(GPPOBJ &&o) {
        std::cout << "Moved " << lastPath << std::endl;
        lastPath = std::move(o.lastPath);
        multiData = std::move(o.multiData);
        mtlLib = std::move(o.mtlLib);
        // mtx = std::move(o.mtx);
        vbodata = std::move(o.vbodata);
        data = std::move(o.data);
        mtlLoaded = o.mtlLoaded;
        modelLoaded = o.modelLoaded;
        keepModelDataLoaded = o.keepModelDataLoaded;
    }
    // GPPOBJ(const std::string &path);
    GPPOBJ();
    ~GPPOBJ();
};

#endif
