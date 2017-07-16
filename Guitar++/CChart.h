#pragma once
#ifndef _GUITARPP_CCHART_H_
#define _GUITARPP_CCHART_H_

#include <string>
#include <deque>
#include <sstream>
#include "CPlayer.h"

class CChart
{
	typedef CPlayer::NotesData::Note Note;
	typedef CPlayer::NotesData::NoteInt NoteInt;
	typedef CPlayer::NotesData::plusNote plusNote;

	struct instrumentNotes
	{
		friend class CChart;

		bool present;

		std::deque<Note> gNotes;
		std::deque<plusNote> gPlus;

		instrumentNotes();

		template<class Archive>
		void load(Archive & archive)
		{
			archive(present, gNotes, gPlus);
		}

		template<class Archive>
		void save(Archive & archive) const
		{
			archive(present, gNotes, gPlus);
		}

	private:
		void deducePlusLastNotes();
	};

	std::map<std::string, instrumentNotes> instruments;

	std::string chartFileName, songName, songArtist, songCharter;
	double chartOffset;

	std::deque<Note> BPM;

	double chartResolutionProp;

	bool parseFeebackChart(std::istream &chartStream);

	template<class Archive>
	void load(Archive & archive)
	{
		archive(instruments, chartFileName, songName, songArtist, songCharter, chartOffset, BPM, chartResolutionProp);
	}

	template<class Archive>
	void save(Archive & archive) const
	{
		archive(instruments, chartFileName, songName, songArtist, songCharter, chartOffset, BPM, chartResolutionProp);
	}

public:
	bool open(const std::string &chartFile);
	bool openFromMemory(const char *chart);

	void fillPlayerData(CPlayer &player, const std::string &instrument);

	CChart();
};

#endif