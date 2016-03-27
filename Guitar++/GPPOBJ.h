#pragma once
#ifndef _GUITAR_PP_GPPOBJ_h_
#define _GUITAR_PP_GPPOBJ_h_
#include <string>
#include <vector>
#include "objloader.hpp"

class GPPOBJ{
	std::vector<glm::vec3> vertices;
	std::vector<glm::vec2> uvs;
	std::vector<glm::vec3> normals;

	unsigned int vb, uvb, nvb;

public:
	void draw(unsigned int texture);

	GPPOBJ(const std::string &path);
};


#endif