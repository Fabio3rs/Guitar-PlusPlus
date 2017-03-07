#pragma once
#ifndef _GUITAR_PP_GPPOBJ_h_
#define _GUITAR_PP_GPPOBJ_h_
#include <string>
#include <array>
#include <vector>
#include "objloader.hpp"
#include "CEngine.h"
#include "CLuaH.hpp"

class GPPOBJ{
	std::vector<int8_t> data;
	CEngine::staticDrawBuffer vbodata;

	struct modelPart
	{
		CEngine::staticDrawBuffer vbodata;
		std::vector<int8_t> data;
		unsigned int textureID;

		inline modelPart()
		{
			textureID = 0;
		}
	};

	struct mtl
	{
		std::string name, texture;

	};

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
	bool loadInternalObj(const std::string &path, const std::string &objName = "", bool loadMtlLib = false);

	bool keepModelDataLoaded;

	void draw(unsigned int texture, bool autoBindZeroVBO = true);
	void load(const std::string &path);
	void load(const char *path);
	void reload(const std::string &path = "");
	void unload();


	GPPOBJ(const std::string &path);
	GPPOBJ();
	~GPPOBJ();
};


#endif