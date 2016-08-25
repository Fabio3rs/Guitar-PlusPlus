#pragma once
#ifndef __GUITARPP_CMULTIPLAYER_H__
#define __GUITARPP_CMULTIPLAYER_H__

#include <thread>
#include "CSocket.h"
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <map>
#include "CPlayer.h"

class CMultiplayer
{
	friend class GPPGame;
	friend class CPlayer;

	std::atomic<bool> continueClThread;

	struct playersData
	{
		char name[64];
		int32_t svthrid;
		int32_t keys;
		int32_t lkeys;
		int32_t strum;
		int32_t lstrum;
		int32_t state;
		int32_t inst;
		int32_t ready;
	};

	struct pair
	{
		int32_t f, s;

		pair()
		{
			f = s = 0;
		}
	};

	pair clidentpair;

	static void callbackFun(CServerSock::ServerThreads *th, char *data, size_t size);

	std::vector < playersData > pData;

	std::map <uintptr_t, std::mutex> mutexplayers;
	std::deque < CPlayer > *players;
	std::map < void*, std::pair<int, int> > xrefplayer;

	struct serverInfo
	{
		double musicRunningTime;
		bool startSong;

		inline serverInfo()
		{
			musicRunningTime = 0.0;
			startSong = false;
		}
	};

	serverInfo svi;

	void updatePlayers();

	static void mpThread();
	std::thread instThread;
	bool bIsServer, bInitSucess;

	CServerSock sv;
	CClientSock cl;

	int state;

	template<class T>
	struct callbackGPPInfo{
		std::function<T> fun;
		
		inline callbackGPPInfo(T f)
		{
			fun = f;
		}

		inline callbackGPPInfo() = default;
	};

	callbackGPPInfo<int(void *ptr, char *data, size_t size)> cb;

public:
	inline std::vector < playersData > &getPlData()
	{
		return pData;
	}

	static std::vector < playersData > &sgetPlData();

	inline void setPlayersData(std::deque < CPlayer > &pl)
	{
		players = &pl;
	}

	void connectToServer(CPlayer &p);

	static void sendUpdateToPlayer(uintptr_t target, const std::vector < playersData > &pData);

	inline bool fail(){ return !bInitSucess; };

	void startMPThread();
	bool playerReady();

	template<class T>
	inline void setGameCallback(T f)
	{
		cb.fun = f;
	}

	void initConnections(const std::string &IP, const std::string &port);

	CMultiplayer(bool bIsServer = false);
	~CMultiplayer();
};


#endif
