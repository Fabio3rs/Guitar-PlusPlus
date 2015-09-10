#pragma once
#ifndef _GUITAR_PP_CGAMEPLAY_h_
#define _GUITAR_PP_CGAMEPLAY_h_
// brmodstudio.forumeiros.com

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
	void renderFretBoard(CPlayer &player, double x1, double x2, double x3, double x4, unsigned int Text);
	void renderIndivdualStrikeButton(int id, double pos, unsigned int Texture, int state, CPlayer &player);
	double pos2Alpha(double pos);
	void updatePlayer(CPlayer &player);
	void renderPlayer(CPlayer &player);
	void renderIndivdualNote(int id, double pos, unsigned int Texture, CPlayer &player);
	double getRunningMusicTime(CPlayer &player);
	double time2Position(double Time, CPlayer &player);
	void renderNote(CPlayer::NotesData::Note &note, CPlayer &player);
	void renderIndividualLine(int id, double pos1, double pos2, unsigned int Texture, CPlayer &player);

	void drawBPMLines(CPlayer &Player);
	void drawBPMLine(double position, unsigned int Texture, CPlayer &Player);

	double speedMp, gSpeed;


public:
	void setHyperSpeed(double s);
	void setMusicSpeed(double s);


	fretsPosition fretsText;
	std::string fretsTextures;

	std::deque < CPlayer > players;

	std::string backgroundTexture;

	//static CGamePlay &gamePlay();

	void update();
	void render();


	void resetModule();

	CGamePlay();
};


#endif
