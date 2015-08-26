#pragma once
#ifndef __GUITARPP_CPLAYER_H_
#define __GUITARPP_CPLAYER_H_

#include <string>
#include <cstdint>
#include <deque>
#include <iostream>
#include <algorithm>
#include <cstdint>

enum notesFlags{
	nf_green = 1, nf_red = 2, nf_yellow = 4, nf_blue = 8, nf_orange = 16,
	nf_not_hopo = 32, nf_doing_slide = 64, nf_failed = 128, nf_picked = 256,
	nf_slide = 512, nf_slide_picked = 1024, losted = 2048, plus_mid = 4096, plus_end = 8192
};

enum playerTypes{local_main, local, lan, lan_main};


struct fretsPosition{
	double lineFretSize, columnSize;
	unsigned int Texture;

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

		Texture = 0;
	}
};


class CPlayer{
	std::string playerName;

public:
	static const int notesEnum;

	std::string smartChartSearch(const std::string &path);
	std::string smartSongSearch(const std::string &path);

	class NotesData{
		std::string chartFileName;

	public:
		size_t notePos;
		int64_t lastNotePicked;

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

		std::deque<Note> gNotes;
		std::deque<Note> BPM;

		bool loadChart(const char *chartFile);
		bool loadFeedbackChart(const char *chartFile);
		void unloadChart();

		NotesData();
		~NotesData();
	};

	int playerType;

private:
	/**/

	int combo;

public:
	std::deque<NotesData::Note> buffer;

	void addPointsByNoteDoed();
	void addPointsByDoingLongNote();
	double comboToMultiplier();
	double comboToMultiplierWM();

	double experience;
	int getLevel();
	void resetData();
	void updateControls();
	void breakCombo();

	bool plusEnabled;

	NotesData Notes;

	bool loadSong(const std::string &path);
	bool loadSongOnlyChart(const std::string &path);
	bool canDoHOPO;

	std::string playerSave;

	double startTime;

	CPlayer(const char *name);
};

#endif
