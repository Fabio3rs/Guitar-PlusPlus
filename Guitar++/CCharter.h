#pragma once
#ifndef _GPP_CCHARTER_H_
#define _GPP_CCHARTER_H_

#include "CPlayer.h"
#include "CEngine.h"
#include "CGamePlay.h"
#include "GPPGame.h"
#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>

class CCharter
{
	CGamePlay gpModule;
	double atMusicTime, divTime;
	bool bForwardBackwardK, bLeftRightK;
	bool fretKeys[5];
	int keypts;
	void eraseNulls(CPlayer &player);
	bool processNewNote;
	std::atomic<bool> loading, continueLoading, loadThreadEnd;
	std::atomic<double> readBPMPercent, readBPMAtSeconds;
	std::mutex loadBPMMutex;
	bool backToZero;

	int songAudioID;

	CPlayer::NotesData::Note *workingNote;
	std::deque<CPlayer::NotesData::Note>::iterator workingNoteIt;
	std::deque<CPlayer::NotesData::Note> songBPM;

	void readSongBPM(unsigned int song);
	double getBPMAt(double time);

	static void loadThreadFun(CCharter &ch);
	void internalLoad();

	std::thread loadThread;

public:
	void prepareDemoGamePlay(CGamePlay &gp);

	void preRender();
	void render();
	void renderInfo();
	void renderAll();

	CCharter();
	~CCharter();
};


#endif