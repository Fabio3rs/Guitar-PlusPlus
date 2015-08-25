#pragma once
#ifndef _GUITAR_PP_CGAMEPLAY_h_
#define _GUITAR_PP_CGAMEPLAY_h_

#include <memory>
#include <algorithm>
#include <cmath>
#include <map>
#include <deque>
#include <string>
#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "CPlayer.h"

class CGamePlay{

public:
	std::deque < CPlayer > players;

	std::string backgroundTexture;

	//static CGamePlay &gamePlay();

	void update();
	void render();


	void resetModule();

	CGamePlay();
};


#endif
