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
#include "GPPOBJ.h"
#include "CParticle.h"

class CCharter;

class CGamePlay{
	friend CCharter;

	int preRenderPlayerSEvent, posRenderPlayerSEvent;

	bool bIsACharterGP;

	bool showBPMVlaues;

    double updateLastTimeCalled, lastiFPSUpdated;
    int iFPS, iFPSCount;

	void renderFretBoard(CPlayer &player, double x1, double x2, double x3, double x4, unsigned int Text);
	void renderFretBoardShadow(CPlayer &player, double x1, double x2, double x3, double x4, unsigned int Text);
	//void renderIndivdualStrikeButton(int id, double pos, unsigned int Texture, int state, CPlayer &player);
	void renderIndivdualStrikeButton3D(int id, double pos, unsigned int Texture, double state, CPlayer &player);
	void renderIndivdualStrikeButton3DStrike(int id, double pos, unsigned int Texture, double state, CPlayer &player);
	void renderIndivdualFlame(int id, double pos, unsigned int Texture, int state, double sizeproportion, CPlayer &player);
	double pos2Alpha(double pos);
	void updatePlayer(CPlayer &player, double deltatime);
	void renderPlayer(CPlayer &player);
	void renderIndivdualNote(int id, double pos, unsigned int Texture, int type, CPlayer &player);
	void renderOpenNote(double pos, unsigned int Texture, int type, CPlayer &player);
	void renderIndivdualNoteShadow(int id, double pos, unsigned int Texture, bool tail, CPlayer &player);
	double getRunningMusicTime(CPlayer &player);
	double time2Position(double Time, CPlayer &player);
	void renderNoteNoAdd(CPlayer::NotesData::Note &note, CPlayer &player);
	void renderNote(const CPlayer::NotesData::Note &note, CPlayer &player);
	void renderNotePlayer(CPlayer &player);
	void renderNoteShadow(CPlayer::NotesData::Note &note, CPlayer &player);
	void renderNoteShadowHpStyle(CPlayer &player);
	void renderIndividualLine(int id, double pos1, double pos2, unsigned int Texture, CPlayer &player);
	void renderTimeOnNote(double pos, double time, CPlayer &player);
	void renderHoposLight();
	void addTailToBuffer(const CPlayer::NotesData::Note &note, double pos1, double pos2, double runningTime, CPlayer &player);
	void renderTailsBuffer(CPlayer &player);
	double getBPMAt(CPlayer &player, double time);
	size_t getBPMAtI(CPlayer &player, double time);
	CPlayer::NotesData::Note getBPMAtStruct(CPlayer &player, double time);
	std::vector<CPlayer::NotesData::Note>::iterator getBPMAtIt(CPlayer &player, double time);
	void alertLVLUP(CPlayer &player);
	void alertNotesStreak(CPlayer &player);
	static bool alertTest(CFonts::textAlert &t);

	std::deque <gppVec3f> hopostp;
	CEngine::dTriangleWithAlpha BPMl, fretboardLData;

	lightData hoposLight, plusNoteLight;

	void renderPylmBar();
	void renderPlayerPylmBar(CPlayer &player);

	void drawBPMLines(CPlayer &Player);
	void drawBPMLine(double position, unsigned int Texture, CPlayer &Player);

	double speedMp, gSpeed;

	CEngine &engine;

public:
	bool enableTails, bFretboardLightFading;
	double fretboardLightFade;

	double fretboardPositionCalcByT(double time, double prop, double *max = nullptr);

	static const int notesFlagsConst[5];

	struct lyricLine
	{
		double tstart, tend;
		std::string lyric;
	};

	std::deque <lyricLine> songlyrics;
	size_t songlyricsIndex;

	void renderLyrics();

	void setHyperSpeed(double s);
	void setMusicSpeed(double s);

	inline double getgSpeed() const
	{
		return gSpeed;
	}

	CMenu moduleMenu;
	int exitModuleOpt;

	//fretsPosition fretsText;
	std::string fretsTextures, BPMLineText;
	unsigned int fireText, pfireText;
	int BPMTextID;
	int hopoLightText;

	bool showBPMLines;

	CPlayersContainer_t players;
	std::vector < std::string > chartInstruments;

	inline CPlayer &getPlayer(int id = 0)
	{
		return *players[id];
	}

	inline CPlayer &getBPlayer()
	{
		return *players.back();
	}

	std::string backgroundTexture;

	void loadSongLyrics(const std::string &song);

	//static CGamePlay &gamePlay();

	void update();
	void marathonUpdate();
	void render();

    bool renderBackground();

	bool bRenderHUD;

	void resetModule();
    void startUpdateDelta();

	CGamePlay(const CGamePlay&) = default;
	CGamePlay(CGamePlay&&) = default;
	CGamePlay();
};


#endif
