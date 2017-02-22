#pragma once
#ifndef GUITARPP_CCAMPAING_H
#define GUITARPP_CCAMPAING_H
#include "CPlayer.h"
#include <vector>
#include <map>
#include <deque>
#include <algorithm>
#include <string>
#include <cstdint>
#include "CGuitars.h"

class CCampaing
{

public:
	struct Contract
	{
		int64_t day, hour, timeToAccept;
		double cost, expectedProfit, cancellationFine;
		int numShows;
		bool exclusivity;

		inline Contract()
		{
			day = hour = timeToAccept = 0;
			cost = expectedProfit = cancellationFine = 0.0;
			numShows = 0;
			exclusivity = false;
		}
	};

	struct Show
	{
		std::vector<std::string> songName;
		int64_t points;
		int64_t maxCombo;
		double money;

		inline Show()
		{
			points = maxCombo = 0;
			money = 0.0;
		}
	};

	struct Schedule
	{
		int64_t day, hour;
		double cost, expectedProfit;

		std::string local;
		std::vector<std::string> possibleSongs;

		inline Schedule()
		{
			day = hour = 0;
			cost = expectedProfit = 0.0;
		}
	};

	struct Email
	{
		int64_t day, hour;
		std::string from, subject, text;

		size_t contract;

		inline Email()
		{
			day = hour = 0;
			contract = ~(0uL);
		}
	};

	class CCampaingData
	{
		std::vector<CPlayer> campaingPlayers;
		std::string bandName;

		double money;
		int64_t reputationPoints;

		std::vector <Show> playedSongs;
		std::vector <Schedule> showsSchedule;
		std::vector <std::string> guitars;
		std::vector <Email> emailList;
		std::vector <Contract> contractList;

		CCampaingData();
	};

	static CCampaing &campaingMGR();

	CCampaing();
};

#endif
