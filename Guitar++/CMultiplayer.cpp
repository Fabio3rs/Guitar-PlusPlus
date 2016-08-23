#include "CMultiplayer.h"

CMultiplayer *mpmgr = nullptr;

void callbackFun(CServerSock::ServerThreads *th, char *data, size_t size)
{
	mpmgr->sendUpdateToPlayer(th->ClientSocket, mpmgr->getPlData());
}

void CMultiplayer::sendUpdateToPlayer(uintptr_t target, const std::vector < playersData > pData)
{
	std::lock_guard<std::mutex> m(mpmgr->mutexplayers[target]);
	CServerSock::sendToClient(target, (const char*)&pData[0], pData.size() * sizeof(playersData));
}

void CMultiplayer::updatePlayers()
{
	


}

void CMultiplayer::mpThread()
{
	while (true)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}



}

void CMultiplayer::initConnections(const std::string &IP, const std::string &port)
{
	if (bIsServer)
	{
		sv.init(port.c_str(), callbackFun);
		bInitSucess = sv.success();
	}
	else
	{
		cl.init(IP.c_str(), port.c_str());
	}
	mpmgr = this;

}

CMultiplayer::CMultiplayer(bool bIsServer)
{
	this->bIsServer = bIsServer;
	bInitSucess = true;
	state = 0;
	mpmgr = this;
}


CMultiplayer::~CMultiplayer()
{
	if (instThread.joinable())
		instThread.join();


	mpmgr = nullptr;

}


