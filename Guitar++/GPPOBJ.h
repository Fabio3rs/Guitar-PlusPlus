#pragma once
#ifndef _GUITAR_PP_GPPOBJ_h_
#define _GUITAR_PP_GPPOBJ_h_
#include <string>
#include <array>
#include <vector>
#include "CEngine.h"
#include "CLuaH.hpp"

class GPPOBJ{
	std::vector<int8_t> data;
	CEngine::staticDrawBuffer vbodata;

	bool mtlLoaded;

	struct loadedTextures
	{
		std::string path, name;
	};

	std::map <unsigned int, loadedTextures> loadedTextures;

	struct modelPart
	{
		CEngine::staticDrawBuffer vbodata;
		std::vector<int8_t> data;
		unsigned int textureID;
		std::string mtl;

		inline modelPart()
		{
			textureID = 0;
		}

		inline ~modelPart()
		{
			vbodata.destroy();
		}
	};

	struct mtl
	{
		std::string textureName;
		unsigned int textureID;

		inline mtl()
		{
			textureID = 0;
		}
	};

	std::map<std::string, mtl> mtlLib;

	void loadMtlLibData(const std::string &path, const std::string &file);

	struct modelPartIndex
	{
		std::string name;
		std::deque<unsigned int> vertexIndices;
		std::deque<unsigned int> uvIndices;
		std::deque<unsigned int> normalIndices;
	};

	struct modelData{
		std::deque<gppVec3f> vertices;
		std::deque<gppVec2f> uvs;
		std::deque<gppVec3f> normals;

		inline void clear()
		{
			vertices.clear();
			uvs.clear();
			normals.clear();
		}
	};

	std::deque<modelPart> multiData;

	std::string lastPath;

public:
	bool loadInternalObj(const std::string &path, const std::string &file, const std::string &objName = "", bool loadMtlLib = false);

	bool keepModelDataLoaded;

	void draw(unsigned int texture, bool autoBindZeroVBO = true);
	void load(const std::string &path, const std::string &file);

	gppVec3f boxTestForMtl(const std::string &mtl);
	//void reload(const std::string &path = "");
	void unload();


	//GPPOBJ(const std::string &path);
	GPPOBJ();
	~GPPOBJ();
};


#endif