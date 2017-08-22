#pragma once
#ifndef _GUITARPP_CCHART_H_
#define _GUITARPP_CCHART_H_

#include "CLog.h"
#include "CPlayer.h"
#include <string>
#include <deque>
#include <sstream>
#include <fstream>
#include <map>
#include <cereal/cereal.hpp>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/deque.hpp>

class CChart
{
	friend class cereal::access;
	typedef CPlayer::NotesData::Note Note;
	typedef CPlayer::NotesData::NoteInt NoteInt;
	typedef CPlayer::NotesData::plusNote plusNote;

	struct instrumentNotes
	{
		friend class cereal::access;
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

	struct {
		std::string chartFileName, songName, songArtist, songCharter;
		double chartOffset, chartResolutionProp;

		template<class Archive>
		void load(Archive &archive)
		{
			archive(chartFileName, songName, songArtist, songCharter, chartOffset, chartResolutionProp);
		}

		template<class Archive>
		void save(Archive &archive) const
		{
			archive(chartFileName, songName, songArtist, songCharter, chartOffset, chartResolutionProp);
		}
	} chartData;

	std::vector<Note> BPM;

	bool parseFeebackChart(std::istream &chartStream);

	template<class Archive>
	void load(Archive &archive)
	{
		archive(instruments, BPM, chartData);
	}

	template<class Archive>
	void save(Archive &archive) const
	{
		archive(instruments, BPM, chartData);
	}

public:
	bool compileGppChart(const std::string &fileName) const;

	static const int notesEnum, notesEnumWithOpenNotes;

	bool open(const std::string &chartFile);
	bool openFromMemory(const char *chart);

	void fillPlayerData(CPlayer &player, const std::string &instrument);

	CChart();
	~CChart() noexcept
	{

	}
};

#endif