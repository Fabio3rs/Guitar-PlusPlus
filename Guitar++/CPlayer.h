#pragma once
// brmodstudio.forumeiros.com
#ifndef __GUITARPP_CPLAYER_H_
#define __GUITARPP_CPLAYER_H_

#include <string>
#include <cstdint>
#include <deque>
#include <iostream>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include "CEngine.h"
#include "CParticle.h"

class CCharter;

enum notesFlags{
	nf_green = 1, nf_red = 2, nf_yellow = 4, nf_blue = 8, nf_orange = 16,
	nf_not_hopo = 32, nf_doing_slide = 64, nf_failed = 128, nf_picked = 256,
	nf_slide = 512, nf_slide_picked = 1024, losted = 2048, plus_mid = 4096, plus_end = 8192, hopontstrmmd = 16384, strmstlrc = 32768
};

enum playerTypes{local_main, local, lan, lan_main};


struct fretsPosition{
	double lineFretSize, columnSize;
	unsigned int notesTexture, strikeLineTexture;

	struct ps {
		int x, y;

		inline ps(int x, int y) {
			this->x = x;
			this->y = y;
		}

		inline ps() { }
	};

	ps FretIMGPos[4 * 8];

	static int sAIDTI(int state, int id)
	{
		return (state + 1) * 5 + id;
	}

	fretsPosition()
	{
		lineFretSize = 1.0 / 4.0;
		columnSize = 1.0 / 8.0;

		FretIMGPos[sAIDTI(0, 0)] = ps(0, 0);
		FretIMGPos[sAIDTI(0, 1)] = ps(1, 0);
		FretIMGPos[sAIDTI(0, 2)] = ps(2, 0);
		FretIMGPos[sAIDTI(0, 3)] = ps(3, 0);
		FretIMGPos[sAIDTI(0, 4)] = ps(4, 0);

		FretIMGPos[sAIDTI(1, 0)] = ps(5, 0);
		FretIMGPos[sAIDTI(1, 1)] = ps(6, 0);
		FretIMGPos[sAIDTI(1, 2)] = ps(7, 0);
		FretIMGPos[sAIDTI(1, 3)] = ps(0, 1);
		FretIMGPos[sAIDTI(1, 3)] = ps(0, 1);
		FretIMGPos[sAIDTI(1, 4)] = ps(1, 1);

		FretIMGPos[sAIDTI(2, 0)] = ps(2, 1);
		FretIMGPos[sAIDTI(2, 1)] = ps(3, 1);
		FretIMGPos[sAIDTI(2, 2)] = ps(4, 1);
		FretIMGPos[sAIDTI(2, 3)] = ps(5, 1);
		FretIMGPos[sAIDTI(2, 4)] = ps(6, 1);

		FretIMGPos[sAIDTI(3, 0)] = ps(7, 1);
		FretIMGPos[sAIDTI(3, 1)] = ps(0, 2);
		FretIMGPos[sAIDTI(3, 2)] = ps(1, 2);
		FretIMGPos[sAIDTI(3, 3)] = ps(2, 2);
		FretIMGPos[sAIDTI(3, 4)] = ps(3, 2);

		FretIMGPos[sAIDTI(4, 0)] = ps(4, 2);
		FretIMGPos[sAIDTI(4, 1)] = ps(5, 2);
		FretIMGPos[sAIDTI(4, 2)] = ps(6, 2);
		FretIMGPos[sAIDTI(4, 3)] = ps(7, 2);
		FretIMGPos[sAIDTI(4, 4)] = ps(0, 3);

		notesTexture = strikeLineTexture = 0;
	}
};


class CPlayer{
	friend CCharter;
	std::string playerName;

public:
	static const int notesEnum;

	std::string smartChartSearch(const std::string &path);
	std::string smartSongSearch(const std::string &path);

	class NotesData{
		friend CCharter;
		std::string chartFileName;

		void deducePlusLastNotes();

	public:
		size_t notePos, plusPos;
		int64_t lastNotePicked;
		std::string instrument;

		std::string songName, songArtist, songCharter;
		std::string songFullPath;

		double chartResolutionProp;

		double fretsNotePickedTime[5];
		bool inLongNote[5];
		int longNoteComb;
		int64_t longNoteID[5];

		struct Note{
			double time, lTime/*, unmodifiedTime*/;
			int type;

			inline bool operator <(const Note &b) const{
				return time < b.time;
			}

			inline bool operator >(const Note &b) const{
				return time > b.time;
			}

			Note(){
				time = 0.0;
				lTime = 0.0;
				//unmodifiedTime = 0.0;
				type = 0;
			}
		};

		struct plusNote{
			double time, lTime/*, unmodifiedTime*/;
			int type;

			int64_t firstNote, lastNote;

			inline bool operator <(const Note &b) const{
				return time < b.time;
			}

			inline bool operator >(const Note &b) const{
				return time > b.time;
			}

			plusNote(){
				time = 0.0;
				lTime = 0.0;
				//unmodifiedTime = 0.0;
				type = 0;
				firstNote = -1;
				lastNote = 0;
			}
		};

		std::deque<Note> gNotes;
		std::deque<Note> BPM;
		std::deque<plusNote> gPlus;

		bool loadChart(const char *chartFile);
		bool loadFeedbackChart(const char *chartFile);
		void unloadChart();

		NotesData();
		~NotesData();
	};

	int playerType;


	struct MPInfo
	{
		void *i;
		bool ready;

		inline MPInfo()
		{
			i = 0;
			ready = false;
		}

	} multiPlayerInfo;

private:
	/**/

	double points;
	int64_t combo;

public:
	int songAudioID;
	int instrumentSound;

	bool enableBot;

	bool aError;

	int lastHOPO;

	std::string plname;

	int BPMNowBuffer;

	std::deque<NotesData::Note> buffer;

	double spectrumLines[8];

	double musicRunningTime;

	bool lastFretsPressed[5];
	bool fretsPressed[5];
	int64_t notesSlide[5];

	int64_t strklinent;
	double strklinenttime;

	bool palhetaKeyLast;
	bool palhetaKey;

	void addPointsByNoteDoed();
	void addPointsByDoingLongNote();
	double comboToMultiplier();
	double comboToMultiplierWM();

	double experience;
	int getLevel();
	void resetData();
	void updateControls();
	void breakCombo();
	void processError();

	void doNote(int64_t i);

	bool plusEnabled;

	NotesData Notes;

	void update();

	void instrumentPlay();
	void instrumentPause();
	void muteInstrument();
	void unmuteInstrument();

	int getFretsPressedFlags();
	int getLastFretsPressedFlags();

	bool loadSong(const std::string &path);
	bool loadSongOnlyChart(const std::string &path);
	bool canDoHOPO;

	double rangle;

	std::string playerSave;

	double startTime, plusThunterStrikeStart, plusPower, plusParticleEffectPosition, maxPlusPower, plusLoadF, plusLoadB, plusLoadInterval;
	double publicAprov, maxPublicAprov, correctNotes;

	unsigned int plusCircleBuffer, plusLoadBuffer, publicApprovBuffer, correctNotesBuffer, multiplierBuffer;

	int64_t getCombo();
	int64_t getPoints();

	CEngine::cameraSET playerCamera;

	bool bRenderP, bUpdateP;

	CParticle playerParticles;

	CPlayer(const char *name);
};

#endif
