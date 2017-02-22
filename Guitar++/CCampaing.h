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
#include "CLuaFunctions.hpp"
#include <cereal/cereal.hpp>
#include <cereal/archives/portable_binary.hpp>

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
		std::string local;
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

	struct News
	{
		int64_t day;
		std::string text;

		News()
		{
			day = 0;
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
		std::vector <News> newsList;

		std::map <std::string, CLuaH::customParam> scriptVars;

		CCampaingData();
	};

	static int getCampaingList(lua_State *L);

	static int registerLuaFunctions(lua_State *L);

	static CCampaing &campaingMGR();

private:
	CCampaing();

	CCampaing(CCampaing&) = delete;
	CCampaing(CCampaing&&) = delete;
};

#endif
