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
	static int campaingLoadSE, campaingInitSE, campaingMenuPreFunSE, campaingMenuMidFunSE, campaingMenuPosFunSE, campaingDScreenUpdateSE, campaingMainMenuOpenSE;

	bool campaingLoaded;
	std::string loadedCampaingFilepath;
	const std::string campaingScriptsDirectory;

	int numCampaingSaves;
	int menuNovaCampanhaID, menuContinuarCampanhaID;

	CLuaH::scriptStorage campaingScripts;

	bool continueInDrawScreen, keepMenuStack;

	int campaingFunctionGotoID;

	CMenu campaingMenu;

public:
	bool loadCampaingF(const std::string &filepath);
	bool saveCampaingF();

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
		std::deque<std::string> songName;
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
		std::deque<std::string> possibleSongs;

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

	struct luaScriptSave
	{
		std::string name;
		std::vector<char> byteCode;
		CLuaH::customParam scriptVars;

		template<class Archive>
		void load(Archive &archive)
		{
			archive(name, byteCode, scriptVars);
		}

		template<class Archive>
		void save(Archive &archive) const
		{
			archive(name, byteCode, scriptVars);
		}
	};

	class CCampaingData
	{
		friend class CCampaing;
		std::string mode;
		std::deque<CPlayer> campaingPlayers;
		std::string bandName;

		double money, playedTime;
		int64_t reputationPoints;

		std::deque <Show> playedSongs;
		std::deque <Schedule> showsSchedule;
		std::deque <std::string> guitars;
		std::deque <Email> emailList;
		std::deque <Contract> contractList;
		std::deque <News> newsList;

		std::deque <luaScriptSave> scripts;

	public:
		template<class Archive>
		void load(Archive &archive)
		{
			archive(mode, campaingPlayers, bandName, money, playedTime, reputationPoints, playedSongs, showsSchedule, guitars, emailList, contractList, newsList, scripts);
		}

		template<class Archive>
		void save(Archive &archive) const
		{
			archive(mode, campaingPlayers, bandName, money, playedTime, reputationPoints, playedSongs, showsSchedule, guitars, emailList, contractList, newsList, scripts);
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

	static int registerLuaFunctions(CLuaH::luaState &Lstate);

	static CCampaing &campaingMGR();

	int newCampaing();
	int continueCampaing(const std::string &path);

	int campaingMainMenu(CMenu &menu);

	void loadCampaingModes();

	static std::deque<std::string> listCampaingSaves();

protected:
	static int getBandName(lua_State *L);
	static int getCampaingMode(lua_State *L);
	static int getBandMoney(lua_State *L);
	static int getBandReputationPoints(lua_State *L);
	static int isCampaingLoaded(lua_State *L);
	static int getLoadedCampaingPath(lua_State *L);
	static int keepCampaingMenuStack(lua_State *L);
	static int exitCampaingScreen(lua_State *L);

	static int campaingLoop();
	static int campaingDrawScreen();

private:
	static int openCampaingMenuCallback(CMenu &menu);
	static int cotinueCampaingOptCallback(CMenu &menu, CMenu::menuOpt &opt);
	static void campaingPlayLoop(const std::string &n);

	CCampaingData campaingNow;
	std::string mainMenu;
	std::deque<std::string> playSongsList;

	CCampaing();

	CCampaing(CCampaing&) = delete;
	CCampaing(CCampaing&&) = delete;
};

#endif
