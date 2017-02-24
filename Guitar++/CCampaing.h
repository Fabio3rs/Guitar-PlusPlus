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
#include "CSaveSystem.h"
#include "CMenu.h"

class CCampaing
{
	bool campaingLoaded;
	std::string loadedCampaingFilepath;
	const std::string campaingScriptsDirectory;

	int numCampaingSaves;
	int menuNovaCampanhaID, menuContinuarCampanhaID;

	bool loadCampaingF(const std::string &filepath);
	bool saveCampaingF();

	std::deque <CLuaH::luaScript> campaingScripts;

public:
	struct Contract
	{
		int64_t day, hour, timeToAccept;
		double cost, expectedProfit, cancellationFine;
		int numShows;
		bool exclusivity;

		template<class Archive>
		void load(Archive &archive)
		{
			archive(day, hour, timeToAccept, cost, expectedProfit, cancellationFine, numShows, exclusivity);
		}

		template<class Archive>
		void save(Archive &archive) const
		{
			archive(day, hour, timeToAccept, cost, expectedProfit, cancellationFine, numShows, exclusivity);
		}

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
		double money, cost;
		bool openBar;

		template<class Archive>
		void load(Archive &archive)
		{
			archive(songName, local, points, maxCombo, money, cost, openBar);
		}

		template<class Archive>
		void save(Archive &archive) const
		{
			archive(songName, local, points, maxCombo, money, cost, openBar);
		}

		inline Show()
		{
			points = maxCombo = 0;
			money = cost = 0.0;
			openBar = false;
		}
	};

	struct Schedule
	{
		int64_t day, hour;
		double cost, expectedProfit;

		std::string local;
		std::vector<std::string> possibleSongs;

		template<class Archive>
		void load(Archive &archive)
		{
			archive(day, hour, cost, local, possibleSongs);
		}

		template<class Archive>
		void save(Archive &archive) const
		{
			archive(day, hour, cost, local, possibleSongs);
		}

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

		template<class Archive>
		void load(Archive &archive)
		{
			archive(day, hour, from, subject, text, contract);
		}

		template<class Archive>
		void save(Archive &archive) const
		{
			archive(day, hour, from, subject, text, contract);
		}

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

		template<class Archive>
		void load(Archive &archive)
		{
			archive(day, text);
		}

		template<class Archive>
		void save(Archive &archive) const
		{
			archive(day, text);
		}

		News()
		{
			day = 0;
		}
	};

	class CCampaingData
	{
		friend class CCampaing;
		std::string mode;
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

	public:
		template<class Archive>
		void load(Archive &archive)
		{
			archive(mode, campaingPlayers, bandName, money, reputationPoints, playedSongs, showsSchedule, guitars, emailList, contractList, newsList, scriptVars);
		}

		template<class Archive>
		void save(Archive &archive) const
		{
			archive(mode, campaingPlayers, bandName, money, reputationPoints, playedSongs, showsSchedule, guitars, emailList, contractList, newsList, scriptVars);
		}
		
		CCampaingData();
	};

	struct CampaingMode
	{
		std::string name, author, description;

		CampaingMode()
		{

		}
	};

	std::map <std::string, CampaingMode> campaingModes;

	static int getCampaingList(lua_State *L);

	static int registerLuaFunctions(lua_State *L);

	static CCampaing &campaingMGR();

	int newCampaing();
	int continueCampaing(const std::string &path);

	int campaingMenu();
	int campaingMainMenu(CMenu &menu);

	void loadCampaingModes();

	static std::deque<std::string> listCampaingSaves();

private:
	static int openCampaingMenuCallback(CMenu &menu);
	static int cotinueCampaingOptCallback(CMenu::menuOpt &opt);

	CCampaingData campaingNow;

	CCampaing();

	CCampaing(CCampaing&) = delete;
	CCampaing(CCampaing&&) = delete;
};

#endif
