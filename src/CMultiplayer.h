#pragma once
#ifndef __GUITARPP_CMULTIPLAYER_H__
#define __GUITARPP_CMULTIPLAYER_H__

#include <thread>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <memory>
#include <map>

#include "CSocket.h"

enum clientPackets{GPPCLI_AUTH, GPPCLI_QUERY_PLAYERS, GPPCLI_SET_MY_DATA, GPPCLI_LOGOUT};
enum serverPackets{GPPSV_OK, GPPSV_SENDSONGDATA, GPPSV_SENDPLAYERSDATA, GPPSV_SIGNALSONGEVENT};

struct gppMPSimplePacket
{
	int32_t id;
	int32_t size;
};

class CMultiplayer
{
	CServerSock sv;
	struct playersFrameData
	{
		int32_t svthrid;
		int32_t status;
		int32_t keys;
		int32_t lkeys;
		int32_t strum;
		int32_t lstrum;
		int32_t state;
		int32_t inst;
		int32_t points;
		int32_t combo;
		int32_t ping;
	};

	class svPlayersData
	{
		std::string name;
		uint64_t connectionId;
		playersFrameData data;

		svPlayersData() : connectionId(0)
		{

		}
	};

	std::deque<svPlayersData> svPlData;

	std::atomic<bool> continueClThread;
	std::atomic<bool> serverRunning;

	std::atomic<bool> notifyTest;

	static void clientmgrthread(CMultiplayer *mp, socket_unique &&socketid);
	static void mpclientconnectcb(CServerSock *sv, socket_unique &&socketid);

	std::thread testclientthr;

	std::string svSong;

	//CPlayersContainer_t svPlayers;
	
public:
	void NotifyAll();
	
	bool startServer(const char *port);
	void stopServer();

	SOCKET clientConnect(const char *host, const char *port);

	CMultiplayer();
	~CMultiplayer();
};

#ifdef _WIN32
#include "CPlayer.h"
#include "CSocket.h"
#define COMPILEMP

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
		size_t f, s;

		pair()
		{
			f = s = 0;
		}
	};

	pair clidentpair;

	static void callbackFun(CServerSock::ServerThreads *th, std::unique_ptr<char[]> &data, size_t size);

	std::vector < playersData > pData;

	std::map <uintptr_t, std::mutex> mutexplayers;
	CPlayersContainer_t *players;
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

	inline void setPlayersData(CPlayersContainer_t &pl)
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

#endif
