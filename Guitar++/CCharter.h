#pragma once
#ifndef _GPP_CCHARTER_H_
#define _GPP_CCHARTER_H_

#include "CPlayer.h"
#include "CEngine.h"
#include "CGamePlay.h"
#include "GPPGame.h"

class CCharter
{
	CGamePlay gpModule;
	double atMusicTime, divTime;
	bool bForwardBackwardK, bLeftRightK;
	bool fretKeys[5];
	int keypts;
	void eraseNulls(CPlayer &player);
	bool processNewNote;

	int songAudioID;

	CPlayer::NotesData::Note *workingNote;
	std::deque<CPlayer::NotesData::Note>::iterator workingNoteIt;

public:
	void prepareDemoGamePlay(CGamePlay &gp);

	void preRender();
	void render();
	void renderInfo();
	void renderAll();

	CCharter();
};


#endif