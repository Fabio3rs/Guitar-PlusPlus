#pragma once
#ifndef _CPARTICLE_H_
#define _CPARTICLE_H_
#include <deque>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <utility>
#include "CEngine.h"

class CParticle{

public:
	struct particleData
	{
		int qtd;
		double x, y, z, ax, ay, az, sx, sy, sz, desac, duration, addedAt, sizex, sizey, sizez;
		unsigned int texture;
	};
	std::deque <particleData> part;
	CEngine::dTriangleWithAlpha partDrawData;
	
	void addParticle(const particleData &pdata);
	void render();

	CParticle();
};



#endif