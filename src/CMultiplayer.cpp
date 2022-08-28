#include "CMultiplayer.h"

static CMultiplayer *server = nullptr;

void CMultiplayer::mpclientconnectcb(CServerSock *sv,
                                     socket_unique &&socketid) {
    std::cout << "connected client";

    char buffer[1024] = {0};

    int64_t readr = read(socketid.get(), buffer, 10);

    std::cout << buffer << std::endl;
    {
        server->testclientthr =
            std::thread(clientmgrthread, server, std::move(socketid));
    }
    std::cout << "aaaaaaaaaaaaaaaaaaaaaaaa"
              << "\n";
}

auto CMultiplayer::startServer(const char *port) -> bool {
    server = this;
    sv.init(port, mpclientconnectcb);

    serverRunning = true;
}

void CMultiplayer::clientmgrthread(CMultiplayer *mp, socket_unique &&socketid) {
    std::cout << "clientmgrthread\n";
    while (mp->serverRunning) {
        if (mp->notifyTest) {
            std::cout << "writenotify\n";
            write(socketid.get(), "notify", sizeof("notify"));
            std::cout << "writednotify\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

auto CMultiplayer::clientConnect(const char *host, const char *port) -> SOCKET {
    std::cout << "SOCKET CMultiplayer::clientConnect(const char *host, const "
                 "char *port)\n";
    CClientSock cli(host, std::stoi(port));

    cli.send("aaaaaaaaaaaa", 4);
    char buffer[1024] = {0};
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    if (!server->notifyTest) {
        server->notifyTest = true;
    }

    for (int i = 0; i < 1000; i++) {
        int64_t readr = cli.receive(buffer, sizeof(buffer));
        if (readr > 0) {
            std::cout << readr << "   " << buffer << std::endl;
        } else if (readr < 0) {
            std::cout << "error\n";
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    std::cout << "RETURN SOCKET CMultiplayer::clientConnect(const char *host, "
                 "const char *port)\n";
}

CMultiplayer::CMultiplayer() {
    serverRunning = false;
    continueClThread = false;
}

CMultiplayer::~CMultiplayer() {}

#ifdef _WIN32
#include "CMultiplayer.h"
#include "GPPGame.h"

CMultiplayer *mpmgr = nullptr;

void CMultiplayer::callbackFun(CServerSock::ServerThreads *th,
                               std::unique_ptr<char[]> &dataptr, size_t size) {
    // std::cout << "connect " << th->ClientSocket << "  " << data << std::endl;
    char *data = dataptr.get();

    if (strcmp(data, "connect") == 0 && !th->logged) {
        th->logged = true;
        char *d2 = data + sizeof("connect");

        playersData &tsPlayerData = *(CMultiplayer::playersData *)d2;

        std::lock_guard<std::mutex> m(mpmgr->mutexplayers[th->ClientSocket]);
        auto &pData = mpmgr->getPlData();

        mpmgr->xrefplayer[th].first = pData.size();

        pData.push_back(tsPlayerData);
        std::string name(tsPlayerData.name,
                         tsPlayerData.name +
                             sizeof(tsPlayerData.name) / sizeof(char));

        {
            std::lock_guard<std::mutex> l(GPPGame::playersMutex);

            mpmgr->xrefplayer[th].second = mpmgr->players->size();

            mpmgr->players->push_back(
                std::make_unique<CPlayer>(CPlayer(name.c_str())));
            // mpmgr->players->back().multiPlayerInfo.i =
            // (void*)th->ClientSocket;
        }

        pair playerpairdata;

        playerpairdata.f = mpmgr->xrefplayer[th].first;
        playerpairdata.s = mpmgr->xrefplayer[th].second;

        CServerSock::sendToClient(th->ClientSocket, (char *)&playerpairdata,
                                  sizeof(pair));
    } else if (strcmp(data, "getupdate") == 0) {
        std::lock_guard<std::mutex> m(mpmgr->mutexplayers[th->ClientSocket]);
        mpmgr->sendUpdateToPlayer(th->ClientSocket, mpmgr->getPlData());
    } else if (strcmp(data, "sendupdate") == 0) {
        char *d2 = data + sizeof("sendupdate");

        playersData &tsPlayerData = *(CMultiplayer::playersData *)d2;

        std::lock_guard<std::mutex> m(mpmgr->mutexplayers[th->ClientSocket]);
        auto &pData = mpmgr->getPlData();

        pData[mpmgr->xrefplayer[th].first] = tsPlayerData;
        {
            std::lock_guard<std::mutex> l(GPPGame::playersMutex);
            (*mpmgr->players)[mpmgr->xrefplayer[th].second]
                ->multiPlayerInfo.ready = tsPlayerData.ready;
        }
    }
}

void CMultiplayer::sendUpdateToPlayer(uintptr_t target,
                                      const std::vector<playersData> &pData) {
    char temp[4096];

    memcpy(temp, &mpmgr->svi, sizeof(mpmgr->svi));

    char *t2 = temp + sizeof(mpmgr->svi);

    memcpy(t2, (const char *)&pData[0], pData.size() * sizeof(playersData));

    CServerSock::sendToClient(
        target, temp, pData.size() * sizeof(playersData) + sizeof(mpmgr->svi));
}

void CMultiplayer::updatePlayers() {}

void CMultiplayer::connectToServer(CPlayer &p) {
    char test[2048];
    cl.receiveDataFromServer(test, sizeof(test));

    strcpy(test, "connect");

    playersData &pd = *(CMultiplayer::playersData *)(test + sizeof("connect"));

    strcpy(pd.name, p.plname.c_str());
    pd.ready = 0;
    pd.strum = 0;

    cl.sendToServer(test, sizeof("connect") + sizeof(playersData));

    int retsize = cl.receiveDataFromServer(test, sizeof(test));
    clidentpair = *(pair *)test;
    // std::cout << "  \n  " << retsize << "connect result " << std::endl;
}

bool CMultiplayer::playerReady() {
    auto &pData = getPlData();
    if (pData.size() > clidentpair.f) {
        pData[clidentpair.f].ready = true;

        return svi.startSong;
    }

    // std::cout << pData.size() << std::endl;
    return false;
}

std::vector<CMultiplayer::playersData> &CMultiplayer::sgetPlData() {
    return (mpmgr->pData);
}

void CMultiplayer::mpThread() {
    char test[8192];

    int i = 0;

    while (mpmgr->continueClThread) {
        strcpy(test, "getupdate");

        mpmgr->cl.sendToServer(test, sizeof("getupdate"));

        int retsize = mpmgr->cl.receiveDataFromServer(test, sizeof(test));
        memcpy(&mpmgr->svi, test, sizeof(mpmgr->svi));

        char *t02 = test + sizeof(mpmgr->svi);
        // mpmgr->cl.sendToServer(test, sizeof("okgetupdate"));

        // retsize = mpmgr->cl.receiveDataFromServer(test, sizeof(test));

        auto &pData = mpmgr->getPlData();
        pData.clear();
        // pData.emplace_back((playersData*)test, (playersData*)(((char*)test) +
        // retsize));
        pData.insert(
            pData.begin(), (playersData *)t02,
            (playersData *)(((char *)t02) + retsize - sizeof(mpmgr->svi)));

        pData[mpmgr->clidentpair.f].keys =
            (*mpmgr->players)[0]->getFretsPressedFlags();
        pData[mpmgr->clidentpair.f].lkeys =
            (*mpmgr->players)[0]->getLastFretsPressedFlags();
        pData[mpmgr->clidentpair.f].strum = palhetaNpKey;
        pData[mpmgr->clidentpair.f].lstrum =
            (*mpmgr->players)[0]->palhetaKeyLast;

        if (i < 100 && mpmgr->svi.startSong) {
            (*mpmgr->players)[0]->musicRunningTime =
                mpmgr->svi.musicRunningTime;
            ++i;
        }

        strcpy(test, "sendupdate");
        char *t2 = test + sizeof("sendupdate");

        memcpy(t2, &(pData[mpmgr->clidentpair.f]), sizeof(playersData));

        mpmgr->cl.sendToServer(test,
                               sizeof("sendupdate") + sizeof(playersData));

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void CMultiplayer::startMPThread() { instThread = std::thread(mpThread); }

void CMultiplayer::initConnections(const std::string &IP,
                                   const std::string &port) {
    if (bIsServer) {
        sv.init(port.c_str(), callbackFun);
        bInitSucess = sv.success();
        std::cout << "server status  " << bInitSucess << std::endl;
    } else {
        cl.init(IP.c_str(), port.c_str());
    }
    mpmgr = this;
}

CMultiplayer::CMultiplayer(bool bIsServer) {
    this->bIsServer = bIsServer;
    continueClThread = true;
    bInitSucess = true;
    state = 0;
    mpmgr = this;
}

CMultiplayer::~CMultiplayer() {
    if (bIsServer)
        sv.stop();

    continueClThread = false;

    if (instThread.joinable())
        instThread.join();

    mpmgr = nullptr;
}
#endif
