#pragma once
#ifndef _GUITAR_PP_GPPOBJ_h_
#define _GUITAR_PP_GPPOBJ_h_
#include <string>
#include <vector>
#include "objloader.hpp"
#include "CEngine.h"
#include "CLuaH.hpp"

class GPPOBJ{
	std::vector<int8_t> data;
	CEngine::staticDrawBuffer vbodata;
	std::string lastPath;

public:
	void draw(unsigned int texture, bool autoBindZeroVBO = true);
	void load(const std::string &path);
	void reload(const std::string &path = "");


	GPPOBJ(const std::string &path);
	GPPOBJ();
};


#endif