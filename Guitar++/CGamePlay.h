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
#include <fstream>
#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "CPlayer.h"
#include "CMenu.h"

class CGamePlay{
	void renderFretBoard(CPlayer &player, double x1, double x2, double x3, double x4, unsigned int Text);
	void renderIndivdualStrikeButton(int id, double pos, unsigned int Texture, int state, CPlayer &player);
	void renderIndivdualStrikeButton3D(int id, double pos, unsigned int Texture, double state, CPlayer &player);
	void renderIndivdualStrikeButton3DStrike(int id, double pos, unsigned int Texture, double state, CPlayer &player);
	void renderIndivdualFlame(int id, double pos, unsigned int Texture, int state, double sizeproportion, CPlayer &player);
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
	struct lyricLine
	{
		double tstart, tend;
		std::string lyric;
	};

	std::deque <lyricLine> songlyrics;
	int songlyricsIndex;

	void renderLyrics();

	void setHyperSpeed(double s);
	void setMusicSpeed(double s);

	CMenu moduleMenu;
	int exitModuleOpt;

	fretsPosition fretsText;
	std::string fretsTextures, BPMLineText;
	unsigned int fireText;
	int BPMTextID;

	bool showBPMLines;

	std::deque < CPlayer > players;
	std::deque < std::string > chartInstruments;

	std::string backgroundTexture;

	void loadSongLyrics(const std::string &song);

	//static CGamePlay &gamePlay();

	void update();
	void render();


	void resetModule();

	CGamePlay();
};


#endif
