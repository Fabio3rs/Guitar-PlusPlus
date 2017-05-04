#pragma once
#ifndef _GUITARPP_CCHART_H_
#define _GUITARPP_CCHART_H_

#include <string>
#include <deque>
#include "CPlayer.h"

class CChart
{
	typedef CPlayer::NotesData::Note Note;
	typedef CPlayer::NotesData::plusNote plusNote;

	struct instrumentNotes
	{
		friend class CChart;

		bool present;

		std::deque<Note> gNotes;
		std::deque<plusNote> gPlus;

		instrumentNotes();

	private:
		void deducePlusLastNotes();
	};

	std::map<std::string, instrumentNotes> instruments;

	std::string chartFileName, songName, songArtist, songCharter;
	double chartOffset;

	std::deque<Note> BPM;

	double chartResolutionProp;

public:
	bool open(const std::string &chartFile);
	bool openFromMemory(const char *chart);

	CChart();
};

#endif