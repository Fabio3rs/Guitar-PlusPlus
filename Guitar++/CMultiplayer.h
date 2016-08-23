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
	struct playersData
	{
		char name[64];
		int32_t svthrid;
		int32_t keys;
		int32_t strum;
		int32_t state;
		int32_t inst;
		int32_t ready;
	};

	std::atomic<std::vector < playersData >> pData;

	std::map <uintptr_t, std::mutex> mutexplayers;
	std::deque < CPlayer > *players;

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
	inline std::atomic<std::vector < playersData >> &getPlData()
	{
		return pData;
	}

	inline void setPlayersData(std::deque < CPlayer > &pl)
	{
		players = &pl;
	}

	static void sendUpdateToPlayer(uintptr_t target, const std::vector < playersData > pData);

	inline bool fail(){ return !bInitSucess; };

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
