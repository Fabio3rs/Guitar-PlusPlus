#include "CGPPFileMGR.h"
#include "GPPGame.h"
#include "CLuaH.hpp"
#include "CEngine.h"
#include <cstdio>
#include <cstdlib>
#include "CLog.h"
#include <dirent.h>
#include "CControls.h"
#include <functional>
#include "CFonts.h"
#include "CCharter.h"
#include "CMultiplayer.h"

std::mutex GPPGame::playersMutex;

/*
Provide default settings for the window
*/
GPPGame::gameWindow GPPGame::getWindowDefaults(bool safeMode)
{
	gameWindow w;

	if (safeMode){
		// Safe mode use reduced configs
		w.w = 800;
		w.h = 600;
		w.AA = 0;
		w.colorBits = 24;

		w.fullscreen = false;

		w.name = "safe mode"; // Explicit "safe mode" on window title
	}else{
		w.w = 0;
		w.h = 0;
		w.AA = 0;
		w.colorBits = 24;

		w.fullscreen = true;

		w.name = ""; // Custom name after "Guitar++"
	}

	w.VSyncMode = 3;

	return w;
}

void GPPGame::helpMenu(const std::string &name)
{
	//std::cout << "aaa\n";



}

void GPPGame::charterModule(const std::string &name)
{
	CCharter cht;

	GPPGame::GuitarPP().HUDText = GPPGame::GuitarPP().loadTexture("data/sprites", "HUD.tga").getTextId();
	GPPGame::GuitarPP().fretboardText = GPPGame::GuitarPP().loadTexture("data/sprites", "fretboard.tga").getTextId();
	GPPGame::GuitarPP().lineText = GPPGame::GuitarPP().loadTexture("data/sprites", "line.tga").getTextId();
	GPPGame::GuitarPP().HOPOSText = -1/*GPPGame::GuitarPP().loadTexture("data/sprites", "HOPOS.tga").getTextId()*/;
	GPPGame::GuitarPP().pylmBarText = GPPGame::GuitarPP().loadTexture("data/sprites", "pylmbar.tga").getTextId();

	auto &game = GPPGame::GuitarPP();
	auto realname = game.getCallBackRealName(name);
	auto &module = game.gameModules[realname];

	if (game.getRunningModule().size() > 0)
	{
		throw gameException("A module is already running: " + name);
	}

	game.setVSyncMode(0);

	game.setRunningModule(realname + "benchmark");

	double sTime = CEngine::engine().getTime();

	std::deque<double> data;

	bool bGDemo = false, escape = true;

	while (CEngine::engine().windowOpened())
	{
		GPPGame::GuitarPP().clearScreen();

		if (CEngine::engine().getKey(GLFW_KEY_ESCAPE))
		{
			if (escape) break;
		}
		else
		{
			escape = true;
		}

		if (CEngine::engine().getKey(' '))
		{
			bGDemo = true;
		}

		if(!bGDemo) cht.renderAll();

		if (bGDemo)
		{
			CGamePlay gp;
			cht.prepareDemoGamePlay(gp);

			bool playSound = true;

			while (CEngine::engine().windowOpened())
			{
				GPPGame::GuitarPP().clearScreen();


				if (CEngine::engine().getKey(GLFW_KEY_ESCAPE))
				{
					bGDemo = false;
					escape = false;
					break;
				}

				gp.update();

				if (playSound)
				{
					CEngine::engine().setSoundTime(gp.players.back().songAudioID, gp.players.back().musicRunningTime);

					for (auto &p : gp.players)
					{
						CEngine::engine().setSoundTime(p.instrumentSound, p.musicRunningTime);
					}

					CEngine::engine().playSoundStream(gp.players.back().songAudioID);

					for (auto &p : gp.players)
					{
						CEngine::engine().playSoundStream(p.instrumentSound);
					}

					CEngine::engine().setSoundVolume(gp.players.back().songAudioID, game.songVolume);

					playSound = false;
				}


				gp.render();


				GPPGame::GuitarPP().renderFrame();
			}


			CEngine::engine().pauseSoundStream(gp.players.back().songAudioID);

			for (auto &p : gp.players)
			{
				CEngine::engine().pauseSoundStream(p.instrumentSound);
			}
		}

		GPPGame::GuitarPP().renderFrame();
	}

	game.setRunningModule("");
}

void GPPGame::benchmark(const std::string &name)
{
	auto &game = GPPGame::GuitarPP();
	auto realname = game.getCallBackRealName(name);
	auto &module = game.gameModules[realname];

	if (game.getRunningModule().size() > 0)
	{
		throw gameException("A module is already running: " + name);
	}

	game.setVSyncMode(0);

	game.setRunningModule(realname + "benchmark");


	module.players.push_back(CPlayer("benchmark"));
	module.players.back().loadSongOnlyChart("TTFAF");
	module.players.back().startTime = CEngine::engine().getTime();

	double sTime = CEngine::engine().getTime();

	/*struct {
	};*/


	std::deque<double> data;

	while (CEngine::engine().windowOpened()) {
		GPPGame::GuitarPP().clearScreen();

		if (CEngine::engine().getKey(GLFW_KEY_ESCAPE))
		{
			break;
		}

		module.update();




		module.render();

		GPPGame::GuitarPP().renderFrame();
	}

	game.setRunningModule("");
}

CMenu &GPPGame::getMenuByName(const std::string &name)
{
	return gameMenus[name];
}

void GPPGame::setRunningModule(const std::string m)
{
	runningModule = m;
}

void GPPGame::loadThread(CGamePlay &module, loadThreadData &l)
{
	std::lock_guard<std::mutex> m(GPPGame::playersMutex);
	std::string song = GuitarPP().songToLoad;
	//module.players[0].loadSongOnlyChart(song);
	//module.players[1].loadSong(song);

	for (auto &p : module.players){
		p.loadSong(song);
	}

	module.loadSongLyrics(song);

	l.processing = false;
}

void GPPGame::loadMarathonThread(CGamePlay &module, loadThreadData &l)
{
	std::lock_guard<std::mutex> m(GPPGame::playersMutex);
	std::vector<CPlayer> tmpPlayers;
	//module.players[0].loadSongOnlyChart(song);
	//module.players[1].loadSong(song);
	for (auto &p : module.players)
	{
		tmpPlayers.push_back("");
	}

	l.listEnd = false;
	l.sendToModulePlayers = false;

	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	while (l.continueThread)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		if (l.loadSong && l.songID < l.songsList.size())
		{
			const std::string &song = l.songsList[l.songID];

			std::cout << "Loading Song " << song << std::endl;

			for (auto &p : tmpPlayers)
			{
				p.loadSong(song);
			}

			l.loadSong = false;
			l.processing = false;
		}
		else if (l.songID >= l.songsList.size())
		{
			l.listEnd = true;
		}

		if (l.sendToModulePlayers && l.songID < l.songsList.size())
		{
			l.processing = true;
			const std::string &song = l.songsList[l.songID];

			module.loadSongLyrics(song);

			for (int i = 0, size = module.players.size(); i < size; ++i)
			{
				module.players[i].releaseSong();
				module.players[i].Notes.unloadChart();

				module.players[i].Notes = std::move(tmpPlayers[i].Notes);
				module.players[i].songAudioID = tmpPlayers[i].songAudioID;
				module.players[i].instrumentSound = tmpPlayers[i].instrumentSound;

				module.players[i].correctNotesMarathon += module.players[i].correctNotes;
				module.players[i].correctNotes = 0;
				module.players[i].strklinent = -1;

				module.players[i].BPMNowBuffer = 0;

				tmpPlayers[i].songAudioID = -1;
				tmpPlayers[i].instrumentSound = -1;

				tmpPlayers[i].resetData();
				tmpPlayers[i].Notes.unloadChart();
			}

			tmpPlayers.clear();

			for (auto &p : module.players)
			{
				tmpPlayers.push_back("");
			}

			l.sendToModulePlayers = false;
			l.loadSong = false;
			l.processing = false;
		}
	}


	//module.loadSongLyrics(song);

	l.processing = false;
}

std::deque<std::string> GPPGame::getDirectory(const char *dir, bool getFiles, bool getDirectories)
{
	std::deque<std::string> result;

	DIR *direntd = opendir(dir);
	dirent *rrd = nullptr;

	if (direntd)
	{
		rrd = readdir(direntd);
		while ((rrd = readdir(direntd)) != nullptr)
		{
			std::string name = rrd->d_name;

			if (name != ".." && name != ".")
			{
				if (getDirectories && (rrd->d_type & DT_DIR) != 0)
				{
					result.push_back(name);
				}

				if (getFiles && (rrd->d_type & DT_DIR) == 0)
				{
					result.push_back(name);
				}
			}
		}
		closedir(direntd);
	}

	return result;
}

void GPPGame::initialLoad()
{
	testobj.load("test", "garage_gpp.obj");

	int itext = 0;

	for (auto &s : strumsTexture3D)
	{
		s = loadTexture("data/sprites", "strum" + std::to_string(itext++) + ".tga").getTextId();
	}

	itext = 0;

	for (auto &s : hopoTexture3D)
	{
		s = loadTexture("data/sprites", "hopo" + std::to_string(itext++) + ".tga").getTextId();
	}

	itext = 0;

	for (auto &s : tapTexture3D)
	{
		s = loadTexture("data/sprites", "tap" + std::to_string(itext++) + ".tga").getTextId();
	}

	itext = 0;

	for (auto &sb : sbaseTexture3D)
	{
		sb = loadTexture("data/sprites", "base" + std::to_string(itext++) + ".tga").getTextId();
	}

	itext = 0;

	for (auto &st : striggerTexture3D)
	{
		st = loadTexture("data/sprites", "trigger" + std::to_string(itext++) + ".tga").getTextId();

		//std::cout << st << std::endl;
	}

	openNoteTexture3D = loadTexture("data/sprites", "Opennote.tga").getTextId();
	openNoteHOPOTexture3D = loadTexture("data/sprites", "OpennoteHOPO.tga").getTextId();
	openNotePTexture3D = loadTexture("data/sprites", "OpennoteP.tga").getTextId();
	openNoteHOPOPTexture3D = loadTexture("data/sprites", "OpennoteHOPOP.tga").getTextId();
}

void GPPGame::initialLoad2()
{
	noteOBJ.load("data/models", "GPP_Note.obj");
	triggerBASEOBJ.load("data/models", "TriggerBase.obj");
	triggerOBJ.load("data/models", "Trigger.obj");
	pylmbarOBJ.load("data/models", "pylmbar.obj");
	openNoteOBJ.load("data/models", "GPP_Opennote.obj");
}

void GPPGame::parseParameters(int argc, char *argv[])
{
	for (int i = 0; i < argc; ++i)
	{
		if (argv[i])
		{
			if (argv[i][0] == '-' || argv[i][0] == '/')
			{
				cmdparams[&((argv[i])[1])] = true;
			}
		}
	}

	setDevMode(cmdparams["dev"]);
}

void GPPGame::setDevMode(bool mode)
{
	devMode = mode;
}

void GPPGame::testClient(const std::string &name)
{
	CPlayer p("pato voador");
	/*
	while (CEngine::engine().windowOpened())
	{
		GPPGame::GuitarPP().clearScreen();

		CFonts::fonts().drawTextInScreenWithBuffer("patos voadores", -0.4, 0.0, 0.1);

		GPPGame::GuitarPP().renderFrame();
	}
	*/



	auto &game = GPPGame::GuitarPP();
	auto realname = game.getCallBackRealName(name);
	auto &module = game.gameModules[realname];

	if (game.getRunningModule().size() > 0)
	{
		throw gameException("A module is already running: " + name);
	}

	module.setHyperSpeed(2.5 * game.hyperSpeed);

	game.setVSyncMode(0);

	game.setRunningModule(realname);
	/*
	{
	module.players.push_back(CPlayer("xi 3"));
	auto &playerCamera = module.players.back().playerCamera;

	//module.players.back().Notes.instrument = "[ExpertDoubleBass]";

	playerCamera.eyex = 0.65;
	playerCamera.eyey = 0.2;
	playerCamera.eyez = 2.3;
	playerCamera.centerx = 1.3;
	playerCamera.centery = -0.2;
	playerCamera.centerz = 0;
	playerCamera.upx = 0;
	playerCamera.upy = 1;
	playerCamera.upz = 0;
	}

	{
	module.players.push_back(CPlayer("xi 2"));

	//module.players.back().Notes.instrument = "[ExpertDoubleBass]";

	auto &playerCamera = module.players.back().playerCamera;

	playerCamera.eyex = -0.65;
	playerCamera.eyey = 0.2;
	playerCamera.eyez = 2.3;
	playerCamera.centerx = -1.3;
	playerCamera.centery = -0.2;
	playerCamera.centerz = 0;
	playerCamera.upx = 0;
	playerCamera.upy = 1;
	playerCamera.upz = 0;
	}
	*/


	module.players.push_back(p);
	//module.players.back().enableBot = GPPGame::GuitarPP().botEnabled;

	//module.players.back().Notes.instrument = "[ExpertDoubleBass]";
	CMultiplayer mp(false);
	mp.initConnections(ip, port);
	mp.setPlayersData(module.players);
	mp.connectToServer(p);


	loadThreadData l;

	l.processing = true;

	std::thread load(loadThread, std::ref(module), std::ref(l));

	while (CEngine::engine().windowOpened() && l.processing)
	{
		GPPGame::GuitarPP().clearScreen();

		CFonts::fonts().drawTextInScreenWithBuffer("loading", -0.4, 0.0, 0.1);

		GPPGame::GuitarPP().renderFrame();
	}

	for (auto &p : module.players)
	{
		p.startTime = CEngine::engine().getTime() + 3.0;
		p.musicRunningTime = -3.0;
	}

	GPPGame::GuitarPP().HUDText = GPPGame::GuitarPP().loadTexture("data/sprites", "HUD.tga").getTextId();
	GPPGame::GuitarPP().fretboardText = GPPGame::GuitarPP().loadTexture("data/sprites", "fretboard.tga").getTextId();
	GPPGame::GuitarPP().lineText = GPPGame::GuitarPP().loadTexture("data/sprites", "line.tga").getTextId();
	GPPGame::GuitarPP().HOPOSText = -1/*GPPGame::GuitarPP().loadTexture("data/sprites", "HOPOS.tga").getTextId()*/;
	GPPGame::GuitarPP().pylmBarText = GPPGame::GuitarPP().loadTexture("data/sprites", "pylmbar.tga").getTextId();

	double startTime = module.players.back().startTime = CEngine::engine().getTime() + 3.0;
	double openMenuTime = 0.0;
	module.players.back().musicRunningTime = -3.0;

	bool enterInMenu = false, esc = false;
	int musicstartedg = 0;

	bool songTimeFixed = false;

	bool botK = false;


	//CLog::log() << std::to_string(module.players.back().enableBot) + "bot que voa";

	std::cout << "Plus in chart: " << module.players.back().Notes.gPlus.size() << std::endl;

	if (module.players.back().Notes.gPlus.size())
	{
		std::cout << "Plus 0 time: " << module.players.back().Notes.gPlus[0].time << std::endl;
		std::cout << "Plus 0 duration: " << module.players.back().Notes.gPlus[0].lTime << std::endl;
	}

	bool firstStartFrame = true;

	auto &playerb = module.players.back();
	std::string songName = playerb.Notes.songName, songArtist = playerb.Notes.songArtist, songCharter = playerb.Notes.songCharter;

	double fadeoutdsc = CEngine::engine().getTime();

	mp.startMPThread();

	std::this_thread::sleep_for(std::chrono::milliseconds(20));

	do
	{
		GPPGame::GuitarPP().clearScreen();

		CFonts::fonts().drawTextInScreenWithBuffer("Getting data", -0.4, 0.0, 0.1);

		GPPGame::GuitarPP().renderFrame();
	} while (CEngine::engine().windowOpened() && !mp.playerReady());

	while (CEngine::engine().windowOpened())
	{
		GPPGame::GuitarPP().clearScreen();

		if (CEngine::engine().getKey(GLFW_KEY_ESCAPE))
		{
			esc = true;
		}
		else if (esc)
		{
			enterInMenu = true;
		}
		else
		{
			enterInMenu = false;
		}



		if (enterInMenu)
		{
			if (musicstartedg == 2)
			{
				for (auto &p : module.players)
				{
					p.instrumentPause();
				}

				CEngine::engine().pauseSoundStream(module.players.back().songAudioID);
				musicstartedg = 0;
			}

			songTimeFixed = false;

			openMenuTime = CEngine::engine().getTime();

			game.openMenus(&module.moduleMenu);

			if (module.moduleMenu.options[module.exitModuleOpt].status & 1)
			{
				module.resetModule();
				break;
			}

			double time = CEngine::engine().getTime();

			for (auto &p : module.players)
			{
				p.startTime += time - openMenuTime;
			}

			enterInMenu = false;
			esc = false;
		}
		else
		{
			if (CEngine::engine().getKey('B'))
			{
				if (!botK)
				{
					botK = true;
					module.players.back().enableBot ^= 1;
				}
			}
			else if (botK)
			{
				botK = false;
			}

			module.update();

			if (!songTimeFixed && module.players.back().musicRunningTime > 0.5)
			{
				CEngine::engine().setSoundTime(module.players.back().songAudioID, module.players.back().musicRunningTime);

				for (auto &p : module.players)
				{
					CEngine::engine().setSoundTime(p.instrumentSound, p.musicRunningTime);
				}

				//std::cout << "First note position: " << module.players.back().Notes.gNotes[0].time << std::endl;

				songTimeFixed = true;
			}

			module.render();
			module.renderLyrics();


			//CFonts::fonts().drawTextInScreen("BASS" + std::to_string(CEngine::engine().getSoundTime(module.players.back().songAudioID)), 0.52, -0.4, 0.1);
			//CFonts::fonts().drawTextInScreen("SONG" + std::to_string(CEngine::engine().getTime() - startTime), 0.52, -0.52, 0.1);

			if (firstStartFrame)
			{
				CEngine::engine().playSoundStream(GuitarPP().startSound);
				firstStartFrame = false;
			}

			double time = CEngine::engine().getTime();

			if (musicstartedg == 0) musicstartedg = 1;

			if ((startTime - time) > 0.0)
			{

				fadeoutdsc = CEngine::engine().getTime();
				CFonts::fonts().drawTextInScreenWithBuffer(std::to_string((int)(startTime - time)), -0.3, 0.0, 0.3);
				musicstartedg = 0;
			}

			double fadeoutdscAlphaCalc = 1.0 - ((CEngine::engine().getTime() - fadeoutdsc) / 3.0);

			if (fadeoutdscAlphaCalc >= 0.0)
			{
				if (fadeoutdscAlphaCalc < 1.0) CEngine::engine().setColor(1.0, 1.0, 1.0, fadeoutdscAlphaCalc);

				CFonts::fonts().drawTextInScreen(songName, -0.9, 0.7, 0.1);
				CFonts::fonts().drawTextInScreen(songArtist, -0.88, 0.62, 0.08);
				CFonts::fonts().drawTextInScreen(songCharter, -0.88, 0.54, 0.08);

				if (fadeoutdscAlphaCalc < 1.0) CEngine::engine().setColor(1.0, 1.0, 1.0, 1.0);
			}

			if (musicstartedg == 1 && module.players.back().musicRunningTime >= 0.0)
			{
				CEngine::engine().setSoundVolume(module.players.back().songAudioID, game.songVolume);
				CEngine::engine().playSoundStream(module.players.back().songAudioID);

				for (auto &p : module.players)
				{
					p.instrumentPlay();
				}

				musicstartedg = 2;
			}
		}




		GPPGame::GuitarPP().renderFrame();
	}

	if (load.joinable()) load.join();




}

void GPPGame::serverModule(const std::string &name)
{
	auto &game = GPPGame::GuitarPP();
	auto realname = game.getCallBackRealName(name);
	auto &module = game.gameModules[realname];
	module.players.clear();

	if (game.getRunningModule().size() > 0)
	{
		throw gameException("A module is already running: " + name);
	}

	CMultiplayer mp(true);
	mp.initConnections(ip, port);
	mp.setPlayersData(module.players);

	module.setHyperSpeed(2.5 * game.hyperSpeed);

	game.setVSyncMode(0);

	game.setRunningModule(realname);

	module.players.push_back(CPlayer("you"));
	module.players.back().playerCamera.centerx = -0.6;
	module.players.back().playerCamera.eyex = -0.4;
	module.players.back().playerHudOffsetX = 1.82;
	module.players.back().playerHudOffsetY = 0.1;
	module.players.back().playerCamera.eyez = 2.55;

	module.players.back().multiPlayerInfo.i = 0;
	module.players.back().multiPlayerInfo.ready = true;

	module.players.back().enableBot = GPPGame::GuitarPP().botEnabled;


	std::atomic<int> state;
	state = 0;

	auto searchPlayerByMPInfo = [&module](void *m)
	{
		for (int i = 0, size = module.players.size(); i < size; i++)
		{
			if (module.players[i].multiPlayerInfo.i == m)
			{
				return i;
			}
		}

		return -1;
	};

	auto mpcallback = [&](void *ptr, char *data, size_t size)
	{
		int p = searchPlayerByMPInfo(ptr);

		/*if (p == -1 && state == 0)
		{
			std::lock_guard<std::mutex> l(playersMutex);
			module.players.push_back(CPlayer("newPlayer"));
			module.players.back().multiPlayerInfo.i = ptr;
			module.players.back().bRenderP = false;
		}*/

		if (state == 1)
		{
			/*if (strcmp(data, "ready") == 0)
			{
				std::lock_guard<std::mutex> l(playersMutex);

				int p = searchPlayerByMPInfo(ptr);

				if (p != -1)
				{
					module.players[p].multiPlayerInfo.ready = true;
				}
			}
			*/
		}

		return 0;
	};

	mp.setGameCallback(mpcallback);

	while (CEngine::engine().windowOpened())
	{
		GPPGame::GuitarPP().clearScreen();

		if (CEngine::engine().getKey('C'))
		{
			state = 1;
			break;
		}

		CFonts::fonts().drawTextInScreenWithBuffer("waiting for players", -0.4, 0.0, 0.1);

		{
			std::lock_guard<std::mutex> l(playersMutex);
			CFonts::fonts().drawTextInScreenWithBuffer(std::to_string(module.players.size()), -0.2, -0.1, 0.1);
		}

		GPPGame::GuitarPP().renderFrame();
	}

	/*
	{
	module.players.push_back(CPlayer("xi 3"));
	auto &playerCamera = module.players.back().playerCamera;

	//module.players.back().Notes.instrument = "[ExpertDoubleBass]";

	playerCamera.eyex = 0.65;
	playerCamera.eyey = 0.2;
	playerCamera.eyez = 2.3;
	playerCamera.centerx = 1.3;
	playerCamera.centery = -0.2;
	playerCamera.centerz = 0;
	playerCamera.upx = 0;
	playerCamera.upy = 1;
	playerCamera.upz = 0;
	}

	{
	module.players.push_back(CPlayer("xi 2"));

	//module.players.back().Notes.instrument = "[ExpertDoubleBass]";

	auto &playerCamera = module.players.back().playerCamera;

	playerCamera.eyex = -0.65;
	playerCamera.eyey = 0.2;
	playerCamera.eyez = 2.3;
	playerCamera.centerx = -1.3;
	playerCamera.centery = -0.2;
	playerCamera.centerz = 0;
	playerCamera.upx = 0;
	playerCamera.upy = 1;
	playerCamera.upz = 0;
	}
	*/

	//module.players.back().Notes.instrument = "[ExpertDoubleBass]";

	loadThreadData l;

	l.processing = true;
	bool allPlayersReady = false;

	std::thread load(loadThread, std::ref(module), std::ref(l));

	module.players.back().playerCamera.centerx = 0.6;
	module.players.back().playerCamera.eyex = 0.4;
	module.players.back().playerCamera.eyez = 2.55;
	module.players.back().playerHudOffsetX = -0.42;
	module.players.back().playerHudOffsetY = 0.1;
	module.players.back().remoteControls = true;

	int r = 0;

	while (CEngine::engine().windowOpened() && (l.processing || !allPlayersReady))
	{
		GPPGame::GuitarPP().clearScreen();

		CFonts::fonts().drawTextInScreenWithBuffer("loading", -0.4, 0.0, 0.1);
		CFonts::fonts().drawTextInScreenWithBuffer("players Ready: " + std::to_string(r), -0.4, -0.2, 0.1);

		{
			std::lock_guard<std::mutex> l(playersMutex);

			allPlayersReady = true;
			r = 0;

			for (int i = 0, size = module.players.size(); i < size; i++)
			{
				if (!module.players[i].multiPlayerInfo.ready)
				{
					allPlayersReady = false;
				}
				else
				{
					++r;
				}
			}
		}

		GPPGame::GuitarPP().renderFrame();
	}

	for (auto &p : module.players)
	{
		p.startTime = CEngine::engine().getTime() + 3.0;
		p.musicRunningTime = -3.0;
		mp.svi.musicRunningTime = p.musicRunningTime;
	}

	GPPGame::GuitarPP().HUDText = GPPGame::GuitarPP().loadTexture("data/sprites", "HUD.tga").getTextId();
	GPPGame::GuitarPP().fretboardText = GPPGame::GuitarPP().loadTexture("data/sprites", "fretboard.tga").getTextId();
	GPPGame::GuitarPP().lineText = GPPGame::GuitarPP().loadTexture("data/sprites", "line.tga").getTextId();
	GPPGame::GuitarPP().HOPOSText = -1/*GPPGame::GuitarPP().loadTexture("data/sprites", "HOPOS.tga").getTextId()*/;
	GPPGame::GuitarPP().pylmBarText = GPPGame::GuitarPP().loadTexture("data/sprites", "pylmbar.tga").getTextId();

	double startTime = module.players.back().startTime = CEngine::engine().getTime() + 3.0;
	double openMenuTime = 0.0;
	module.players.back().musicRunningTime = -3.0;

	bool enterInMenu = false, esc = false;
	int musicstartedg = 0;

	bool songTimeFixed = false;

	//CLog::log() << std::to_string(module.players.back().enableBot) + "bot que voa";

	/*std::cout << "Plus in chart: " << module.players.back().Notes.gPlus.size() << std::endl;

	if (module.players.back().Notes.gPlus.size())
	{
		std::cout << "Plus 0 time: " << module.players.back().Notes.gPlus[0].time << std::endl;
		std::cout << "Plus 0 duration: " << module.players.back().Notes.gPlus[0].lTime << std::endl;
	}*/

	bool firstStartFrame = true;

	auto &playerb = module.players.back();
	std::string songName = playerb.Notes.songName, songArtist = playerb.Notes.songArtist, songCharter = playerb.Notes.songCharter;

	double fadeoutdsc = CEngine::engine().getTime();

	mp.svi.startSong = true;

	while (CEngine::engine().windowOpened())
	{
		GPPGame::GuitarPP().clearScreen();
		mp.svi.musicRunningTime = module.players.back().musicRunningTime;

		if (CEngine::engine().getKey(GLFW_KEY_ESCAPE))
		{
			esc = true;
		}
		else if (esc)
		{
			enterInMenu = true;
		}
		else
		{
			enterInMenu = false;
		}



		if (enterInMenu)
		{
			if (musicstartedg == 2)
			{
				for (auto &p : module.players)
				{
					p.instrumentPause();
				}

				CEngine::engine().pauseSoundStream(module.players.back().songAudioID);
				musicstartedg = 0;
			}

			songTimeFixed = false;

			openMenuTime = CEngine::engine().getTime();

			game.openMenus(&module.moduleMenu);

			if (module.moduleMenu.options[module.exitModuleOpt].status & 1)
			{
				module.resetModule();
				break;
			}

			double time = CEngine::engine().getTime();

			for (auto &p : module.players)
			{
				p.startTime += time - openMenuTime;
			}

			enterInMenu = false;
			esc = false;
		}
		else
		{
			module.update();

			if (!songTimeFixed && (CEngine::engine().getTime() - startTime) > 0.5)
			{
				CEngine::engine().setSoundTime(module.players.back().songAudioID, module.players.back().musicRunningTime);

				for (auto &p : module.players)
				{
					CEngine::engine().setSoundTime(p.instrumentSound, p.musicRunningTime);
				}

				//std::cout << "First note position: " << module.players.back().Notes.gNotes[0].time << std::endl;

				songTimeFixed = true;
			}

			module.render();
			module.renderLyrics();


			//CFonts::fonts().drawTextInScreen("BASS" + std::to_string(CEngine::engine().getSoundTime(module.players.back().songAudioID)), 0.52, -0.4, 0.1);
			//CFonts::fonts().drawTextInScreen("SONG" + std::to_string(CEngine::engine().getTime() - startTime), 0.52, -0.52, 0.1);

			if (firstStartFrame)
			{
				CEngine::engine().playSoundStream(GuitarPP().startSound);
				firstStartFrame = false;
			}

			double time = CEngine::engine().getTime();

			if (musicstartedg == 0) musicstartedg = 1;

			if ((startTime - time) > 0.0)
			{

				fadeoutdsc = CEngine::engine().getTime();
				CFonts::fonts().drawTextInScreenWithBuffer(std::to_string((int)(startTime - time)), -0.3, 0.0, 0.3);
				musicstartedg = 0;
			}

			double fadeoutdscAlphaCalc = 1.0 - ((CEngine::engine().getTime() - fadeoutdsc) / 3.0);

			if (fadeoutdscAlphaCalc >= 0.0)
			{
				if (fadeoutdscAlphaCalc < 1.0) CEngine::engine().setColor(1.0, 1.0, 1.0, fadeoutdscAlphaCalc);

				CFonts::fonts().drawTextInScreen(songName, -0.9, 0.7, 0.1);
				CFonts::fonts().drawTextInScreen(songArtist, -0.88, 0.62, 0.08);
				CFonts::fonts().drawTextInScreen(songCharter, -0.88, 0.54, 0.08);

				if (fadeoutdscAlphaCalc < 1.0) CEngine::engine().setColor(1.0, 1.0, 1.0, 1.0);
			}

			if (musicstartedg == 1)
			{
				CEngine::engine().setSoundVolume(module.players.back().songAudioID, game.songVolume);
				CEngine::engine().playSoundStream(module.players.back().songAudioID);

				for (auto &p : module.players)
				{
					p.instrumentPlay();
				}

				musicstartedg = 2;
			}
		}




		GPPGame::GuitarPP().renderFrame();
	}

	if (load.joinable()) load.join();
}

void GPPGame::startModule(const std::string &name)
{
	auto &game = GPPGame::GuitarPP();
	auto &engine = CEngine::engine();
	auto &fonts = CFonts::fonts();
	auto realname = game.getCallBackRealName(name);
	auto &module = game.gameModules[realname];
	game.gameplayRunningTime = 0.0;

	if (game.getRunningModule().size() > 0)
	{
		throw gameException("A module is already running: " + name);
	}

	std::string song = game.selectSong();
	
	if (song.size() > 0)
	{
		game.songToLoad = song;
	}
	else
	{
		game.setRunningModule("");
		return;
	}

	module.setHyperSpeed(2.5 * game.hyperSpeed);

	game.setVSyncMode(0);

	game.setRunningModule(realname);
	/*
	{
		module.players.push_back(CPlayer("xi 3"));
		auto &playerCamera = module.players.back().playerCamera;

		//module.players.back().Notes.instrument = "[ExpertDoubleBass]";

		playerCamera.eyex = 0.65;
		playerCamera.eyey = 0.2;
		playerCamera.eyez = 2.3;
		playerCamera.centerx = 1.3;
		playerCamera.centery = -0.2;
		playerCamera.centerz = 0;
		playerCamera.upx = 0;
		playerCamera.upy = 1;
		playerCamera.upz = 0;
	}

	{
		module.players.push_back(CPlayer("xi 2"));

		//module.players.back().Notes.instrument = "[ExpertDoubleBass]";

		auto &playerCamera = module.players.back().playerCamera;

		playerCamera.eyex = -0.65;
		playerCamera.eyey = 0.2;
		playerCamera.eyez = 2.3;
		playerCamera.centerx = -1.3;
		playerCamera.centery = -0.2;
		playerCamera.centerz = 0;
		playerCamera.upx = 0;
		playerCamera.upy = 1;
		playerCamera.upz = 0;
	}
	*/


	module.players.push_back(CPlayer("xi"));

	loadThreadData l;

	l.processing = true;

	std::thread load(loadThread, std::ref(module), std::ref(l));

	CLuaH::Lua().runEvent("gameplayLoading");

	while (engine.windowOpened() && l.processing)
	{
		game.clearScreen();

		CFonts::fonts().drawTextInScreenWithBuffer("loading", -0.4, 0.0, 0.1);

		game.renderFrame();
	}

	for (auto &p : module.players)
	{
		p.startTime = engine.getTime() + 3.0;
		p.musicRunningTime = -3.0;
	}

	game.HUDText = game.loadTexture("data/sprites", "HUD.tga").getTextId();
	game.fretboardText = game.loadTexture("data/sprites", "fretboard.tga").getTextId();
	game.lineText = game.loadTexture("data/sprites", "line.tga").getTextId();
	game.HOPOSText = -1/*GPPGame::GuitarPP().loadTexture("data/sprites", "HOPOS.tga").getTextId()*/;
	game.pylmBarText = game.loadTexture("data/sprites", "pylmbar.tga").getTextId();

	double startTime = module.players.back().startTime = engine.getTime() + 3.0;
	double openMenuTime = 0.0;
	module.players.back().musicRunningTime = -3.0;
	module.players.back().guitar = CGuitars::inst().getGuitarIfExists(game.defaultGuitar);

	if(module.players.back().guitar)
		module.players.back().guitar->load();

	bool enterInMenu = false, esc = false;
	int musicstartedg = 0;

	bool songTimeFixed = false;


	module.players.back().enableBot = game.botEnabled;
	module.players.back().playerCamera.centery = 1.0;
	module.players.back().playerCamera.eyez += 2.0;
	//CLog::log() << std::to_string(module.players.back().enableBot) + "bot que voa";

	std::cout << "Plus in chart: " << module.players.back().Notes.gPlus.size() << std::endl;

	if (module.players.back().Notes.gPlus.size())
	{
		std::cout << "Plus 0 time: " << module.players.back().Notes.gPlus[0].time << std::endl;
		std::cout << "Plus 0 duration: " << module.players.back().Notes.gPlus[0].lTime << std::endl;
	}

	bool firstStartFrame = true;
	
	auto &playerb = module.players.back();
	std::string songName = playerb.Notes.songName, songArtist = playerb.Notes.songArtist, songCharter = playerb.Notes.songCharter;

	double fadeoutdsc = engine.getTime();

	while (engine.windowOpened())
	{
		game.clearScreen();

		if (engine.getKey(GLFW_KEY_ESCAPE))
		{
			esc = true;
		}
		else if (esc)
		{
			enterInMenu = true;
		}
		else
		{
			enterInMenu = false;
		}



		if (enterInMenu)
		{
			if (musicstartedg == 2)
			{
				for (auto &p : module.players)
				{
					p.instrumentPause();
				}

				engine.pauseSoundStream(module.players.back().songAudioID);
				musicstartedg = 0;
			}

			songTimeFixed = false;

			openMenuTime = engine.getTime();

			game.openMenus(&module.moduleMenu);

			if (module.moduleMenu.options[module.exitModuleOpt].status & 1)
			{
				module.resetModule();
				break;
			}

			double time = engine.getTime();

			for (auto &p : module.players)
			{
				p.startTime += time - openMenuTime;
			}

			enterInMenu = false;
			esc = false;
		}
		else
		{
			module.update();

			for (auto &p : module.players)
			{
				if (p.targetCamera.centery < p.playerCamera.centery)
				{
					p.playerCamera.centery -= engine.getDeltaTime() * 0.5;
				}

				if (p.targetCamera.centery > p.playerCamera.centery)
				{
					p.playerCamera.centery = p.targetCamera.centery;
				}

				if (p.targetCamera.eyez < p.playerCamera.eyez)
				{
					p.playerCamera.eyez -= engine.getDeltaTime() * 0.75;
				}

				if (p.targetCamera.eyez > p.playerCamera.eyez)
				{
					p.playerCamera.eyez = p.targetCamera.eyez;
				}
			}

			game.gameplayRunningTime = playerb.musicRunningTime;
			game.gamePlayPlusEnabled = playerb.plusEnabled;

			if (!songTimeFixed && (engine.getTime() - startTime) > 0.5)
			{
				engine.setSoundTime(module.players.back().songAudioID, playerb.musicRunningTime);

				for (auto &p : module.players)
				{
					engine.setSoundTime(p.instrumentSound, p.musicRunningTime);
				}

				//std::cout << "First note position: " << module.players.back().Notes.gNotes[0].time << std::endl;

				songTimeFixed = true;
			}




			if (game.drawGamePlayBackground) {


				engine.activate3DRender(true);
				engine.activateLighting(true);

				{
					double centerx = 0.0;
					double centerz = -650.0;

					double rtime = module.players.back().musicRunningTime / 10.0;
					double eyexcam = sin(0) * 1.0 + centerx + sin(rtime) * 1.0;
					double eyezcam = cos(0) * 1.0 + centerz + cos(rtime) * 1.0;

					CEngine::cameraSET usingCamera;

					
					usingCamera.eyex = 3.0 + sin(rtime);
					usingCamera.eyey = 2.5;
					usingCamera.eyez = 1.0 + cos(rtime);
					usingCamera.centerx = 3.0;
					usingCamera.centery = 0.5;
					usingCamera.centerz = -5;
					usingCamera.upx = 0;
					usingCamera.upy = 1;
					usingCamera.upz = 0;
					/*usingCamera.eyex = eyexcam;
					usingCamera.eyey = 130.0;
					usingCamera.eyez = eyezcam;
					usingCamera.centerx = centerx;
					usingCamera.centery = 130.0;
					usingCamera.centerz = centerz;
					usingCamera.upx = 0;
					usingCamera.upy = 1;
					usingCamera.upz = 0;*/

					engine.setCamera(usingCamera);
				}

				{
					lightData l;

					for (auto &t : l.ambientLight)
					{
						t = 0.2f;
					}

					for (auto &t : l.direction)
					{
						t = 2.5f;
					}

					for (auto &t : l.position)
					{
						t = 0.0f;
					}

					for (auto &t : l.specularLight)
					{
						t = 0.5f;
					}

					for (auto &t : l.diffuseLight)
					{
						t = 0.2f;
					}

					l.specularLight[1] = 1.0f;
					l.specularLight[2] = 1.0f;
					l.diffuseLight[0] = 1.0f;
					l.diffuseLight[1] = 1.0f;
					l.ambientLight[3] = 0.1f;

					CEngine::colorRGBToArrayf(0xFFF6ED, l.diffuseLight);

					l.angle = 100.0f;
					l.direction[0] = 3.0f;
					l.direction[1] = -0.5f;
					l.direction[2] = -1.5f;

					l.position[0] = 3.0f;
					l.position[1] = 2.7f;
					l.position[2] = -1.5f;
					l.position[3] = 1.0f;

					engine.activateLight(0, false);
					engine.activateLight(1, true);
					engine.setLight(l, 1);
				}

				/*{
					lightData l;

					for (auto &t : l.ambientLight)
					{
						t = 0.1;
					}

					for (auto &t : l.direction)
					{
						t = 2.5;
					}

					for (auto &t : l.position)
					{
						t = 0.0;
					}

					for (auto &t : l.specularLight)
					{
						t = 1.0;
					}

					for (auto &t : l.diffuseLight)
					{
						t = 0.5;
					}

					l.specularLight[1] = 1.0;
					l.specularLight[2] = 1.0;
					l.specularLight[3] = 0.5;
					l.diffuseLight[3] = 0.5;
					l.ambientLight[3] = 0.1;

					l.angle = 80.0;
					l.direction[0] = 100.0;
					l.direction[1] = -0.5;
					l.direction[2] = 0.0;

					l.position[3] = 0.0;
					l.position[1] = 50.0;
					l.position[2] = 100.5;

					engine.activateLight(0, true);
					engine.setLight(l, 0);
				}*/

				//static const unsigned int cityTextID = game.loadTexture("test", "city.tga").getTextId();

				engine.activateNormals(true);
				game.testobj.draw(0);
				engine.activateNormals(false);

				engine.activateLighting(false);
				engine.activate3DRender(false);

				/*{
					double size = 0.4;

					CEngine::RenderDoubleStruct TempStruct3D;

					static auto hopoLightText = GPPGame::GuitarPP().loadTexture("data/sprites", "hopolight.tga").getTextId();

					double flarex = 2.92, flarey = 2.5, flarez = -1.0;

					TempStruct3D.Text = hopoLightText;
					TempStruct3D.TextureX1 = 0.0;
					TempStruct3D.TextureX2 = 1.0;
					TempStruct3D.TextureY1 = 1.0;
					TempStruct3D.TextureY2 = 0.0;

					//CFonts::fonts().drawTextInScreen(std::to_string(hopostp.size()), 0.0, 0.5, 0.1);

					TempStruct3D.x1 = flarex;
					TempStruct3D.x2 = TempStruct3D.x1 + size;
					TempStruct3D.x3 = TempStruct3D.x1 + size;
					TempStruct3D.x4 = TempStruct3D.x1;

					TempStruct3D.y1 = flarey;
					TempStruct3D.y2 = TempStruct3D.y1;
					TempStruct3D.y3 = flarey + 0.2;
					TempStruct3D.y4 = TempStruct3D.y3;

					TempStruct3D.z1 = flarez + size * 2.0;
					TempStruct3D.z2 = TempStruct3D.z1;
					TempStruct3D.z3 = flarez;
					TempStruct3D.z4 = TempStruct3D.z3;

					CEngine::engine().Render3DQuad(TempStruct3D);
				}*/
				
				engine.matrixReset();

				engine.clear3DBuffer();
				{
					CEngine::cameraSET usingCamera;
					usingCamera.eyex = 0.0;
					usingCamera.eyey = 0.0;
					usingCamera.eyez = 2.3;
					usingCamera.centerx = 0;
					usingCamera.centery = 0;
					usingCamera.centerz = 0.0;
					usingCamera.upx = 0;
					usingCamera.upy = 1;
					usingCamera.upz = 0;

					engine.setCamera(usingCamera);
				}
			}




			module.render();
			module.renderLyrics();


			//CFonts::fonts().drawTextInScreen("BASS" + std::to_string(CEngine::engine().getSoundTime(module.players.back().songAudioID)), 0.52, -0.4, 0.1);
			//CFonts::fonts().drawTextInScreen("SONG" + std::to_string(CEngine::engine().getTime() - startTime), 0.52, -0.52, 0.1);

			if (firstStartFrame)
			{
				CLuaH::Lua().runEvent("firstStartFrame");
				engine.playSoundStream(GuitarPP().startSound);
				firstStartFrame = false;
			}

			double time = engine.getTime();

			if (musicstartedg == 0) musicstartedg = 1;

			if ((startTime - time) > 0.0)
			{

				fadeoutdsc = engine.getTime();
				fonts.drawTextInScreenWithBuffer(std::to_string((int)(startTime - time)), -0.3, 0.0, 0.3);
				musicstartedg = 0;
			}

			double fadeoutdscAlphaCalc = 1.0 - ((engine.getTime() - fadeoutdsc) / 3.0);

			if (fadeoutdscAlphaCalc >= 0.0)
			{
				if (fadeoutdscAlphaCalc < 1.0) engine.setColor(1.0, 1.0, 1.0, fadeoutdscAlphaCalc);

				fonts.drawTextInScreen(songName, -0.9, 0.7, 0.1);
				fonts.drawTextInScreen(songArtist, -0.88, 0.62, 0.08);
				fonts.drawTextInScreen(songCharter, -0.88, 0.54, 0.08);

				if (fadeoutdscAlphaCalc < 1.0) engine.setColor(1.0, 1.0, 1.0, 1.0);
			}

			if (musicstartedg == 1)
			{
				CEngine::engine().setSoundVolume(module.players.back().songAudioID, game.songVolume);
				engine.playSoundStream(module.players.back().songAudioID);

				for (auto &p : module.players)
				{
					p.instrumentPlay();
				}

				musicstartedg = 2;
			}
		}




		game.renderFrame();
	}

	if (load.joinable()) load.join();
}

void GPPGame::startMarathonModule(const std::string &name)
{
	loadThreadData l;

	auto &game = GPPGame::GuitarPP();
	auto &engine = CEngine::engine();
	auto &fonts = CFonts::fonts();
	auto realname = game.getCallBackRealName(name);
	auto &module = game.gameModules[realname];

	if (game.getRunningModule().size() > 0)
	{
		throw gameException("A module is already running: " + name);
	}

	game.HUDText = game.loadTexture("data/sprites", "HUD.tga").getTextId();
	game.fretboardText = game.loadTexture("data/sprites", "fretboard.tga").getTextId();
	game.lineText = game.loadTexture("data/sprites", "line.tga").getTextId();
	game.HOPOSText = -1/*GPPGame::GuitarPP().loadTexture("data/sprites", "HOPOS.tga").getTextId()*/;
	game.pylmBarText = game.loadTexture("data/sprites", "pylmbar.tga").getTextId();

	module.setHyperSpeed(2.5 * game.hyperSpeed);

	game.setVSyncMode(0);

	game.setRunningModule(realname);

	module.players.push_back(CPlayer("xi"));

	l.processing = true;
	l.continueThread = true;
	l.songID = 0;
	l.songsList = getDirectory("./data/songs", false, true);
	l.listEnd = false;
	l.sendToModulePlayers = false;
	l.loadSong = true;


	for (auto &p : l.songsList)
	{
		std::cout << p << std::endl;
	}

	std::thread load(loadMarathonThread, std::ref(module), std::ref(l));

	while (engine.windowOpened() && l.processing)
	{
		game.clearScreen();

		CFonts::fonts().drawTextInScreenWithBuffer("loading marathon 0%", -0.4, 0.0, 0.1);

		game.renderFrame();
	}

	l.sendToModulePlayers = true;
	l.processing = true;

	while (engine.windowOpened() && l.processing)
	{
		game.clearScreen();

		CFonts::fonts().drawTextInScreenWithBuffer("loading marathon 50%", -0.4, 0.0, 0.1);

		game.renderFrame();
	}

	l.songID++;
	l.loadSong = true;

	for (auto &p : module.players)
	{
		p.startTime = engine.getTime() + 3.0;
		p.musicRunningTime = -3.0;
	}

	double startTime = module.players.back().startTime = engine.getTime() + 3.0;
	double openMenuTime = 0.0;
	module.players.back().musicRunningTime = -3.0;
	module.players.back().guitar = CGuitars::inst().getGuitarIfExists(game.defaultGuitar);

	if (module.players.back().guitar)
		module.players.back().guitar->load();

	bool enterInMenu = false, esc = false;
	int musicstartedg = 0;

	bool songTimeFixed = false;

	module.players.back().enableBot = game.botEnabled;
	module.players.back().playerCamera.centery = 1.0;
	module.players.back().playerCamera.eyez += 2.0;

	bool firstStartFrame = true;

	auto &playerb = module.players.back();
	std::string songName = playerb.Notes.songName, songArtist = playerb.Notes.songArtist, songCharter = playerb.Notes.songCharter;

	double fadeoutdsc = engine.getTime();

	bool interval = false, waitingIntervalLoad = false, bOldShowBPMLines = module.showBPMLines;

	double marathonTime = 0.0;

	while (engine.windowOpened())
	{
		game.clearScreen();

		if (engine.getKey(GLFW_KEY_ESCAPE))
		{
			esc = true;
		}
		else if (esc)
		{
			enterInMenu = true;
		}
		else
		{
			enterInMenu = false;
		}



		if (enterInMenu)
		{
			if (musicstartedg == 2)
			{
				for (auto &p : module.players)
				{
					p.instrumentPause();
				}

				engine.pauseSoundStream(module.players.back().songAudioID);
				musicstartedg = 0;
			}

			songTimeFixed = false;

			openMenuTime = engine.getTime();

			game.openMenus(&module.moduleMenu);

			if (module.moduleMenu.options[module.exitModuleOpt].status & 1)
			{
				module.resetModule();
				break;
			}

			double time = engine.getTime();

			for (auto &p : module.players)
			{
				p.startTime += time - openMenuTime;
			}

			enterInMenu = false;
			esc = false;
		}
		else
		{
			if (!interval)
				module.marathonUpdate();
			else
			{
				for (auto &p : module.players)
					p.musicRunningTime += engine.getDeltaTime() * module.getgSpeed();
			}

			bool songChartEnd = true;

			for (auto &p : module.players)
			{
				if (!interval)
				{
					if (songChartEnd)
						songChartEnd = p.isSongChartFinished();
				}

				if (p.targetCamera.centery < p.playerCamera.centery)
				{
					p.playerCamera.centery -= engine.getDeltaTime() * 0.5;
				}

				if (p.targetCamera.centery > p.playerCamera.centery)
				{
					p.playerCamera.centery = p.targetCamera.centery;
				}

				if (p.targetCamera.eyez < p.playerCamera.eyez)
				{
					p.playerCamera.eyez -= engine.getDeltaTime() * 0.75;
				}

				if (p.targetCamera.eyez > p.playerCamera.eyez)
				{
					p.playerCamera.eyez = p.targetCamera.eyez;
				}
			}

			interval = songChartEnd;

			if (interval)
			{
				if (!waitingIntervalLoad)
				{
					l.sendToModulePlayers = true;
					l.processing = true;

					waitingIntervalLoad = true;
					module.showBPMLines = false;
				}
				else
				{
					if (l.processing)
					{
						//// TODO


					}
					else
					{
						double waitTime = 5.0;

						double deltaTol = engine.getDeltaTime();

						if (deltaTol < 0.005)
							deltaTol = 0.005;

						deltaTol /= 2.0;

						double dmaxrntime = 0.0;

						double rningTimeFretCalcMn = module.fretboardPositionCalcByT(module.players.back().musicRunningTime - deltaTol - 4.0, 1.02, &dmaxrntime);
						double rningTimeFretCalcMs = module.fretboardPositionCalcByT(module.players.back().musicRunningTime + deltaTol - 4.0, 1.02);

						rningTimeFretCalcMn = rningTimeFretCalcMn < 0.0 ? -rningTimeFretCalcMn : rningTimeFretCalcMn;
						rningTimeFretCalcMs = rningTimeFretCalcMs < 0.0 ? -rningTimeFretCalcMs : rningTimeFretCalcMs;

						double fretBoardPos = module.fretboardPositionCalcByT(-5 - 4.0, 1.02);

						if (fretBoardPos > 0.0)
						{
							fretBoardPos = dmaxrntime - fretBoardPos;
						}
						else
						{
							fretBoardPos *= -1.0;
						}

						if (fretBoardPos > rningTimeFretCalcMn && fretBoardPos < rningTimeFretCalcMs)
						{
							for (auto &p : module.players)
							{
								p.startTime = engine.getTime() + waitTime;
								p.musicRunningTime = -waitTime;
							}

							startTime = module.players.back().startTime = engine.getTime() + waitTime;
							openMenuTime = 0.0;
							module.players.back().musicRunningTime = -waitTime;

							songTimeFixed = false;
							musicstartedg = 0;

							l.songID++;
							interval = false;
							l.loadSong = true;
							waitingIntervalLoad = false;

							auto &playerb = module.players.back();
							songName = playerb.Notes.songName;
							songArtist = playerb.Notes.songArtist;
							songCharter = playerb.Notes.songCharter;
						}
					}
				}
			}
			else
			{
				module.showBPMLines = bOldShowBPMLines;
				if (!songTimeFixed && (engine.getTime() - startTime) > 0.5)
				{
					engine.setSoundTime(module.players.back().songAudioID, module.players.back().musicRunningTime);

					for (auto &p : module.players)
					{
						engine.setSoundTime(p.instrumentSound, p.musicRunningTime);
					}

					songTimeFixed = true;
				}
			}


			{


				engine.activate3DRender(true);
				engine.activateLighting(true);

				{
					double centerx = 0.0;
					double centerz = -650.0;

					double rtime = marathonTime / 10.0;
					double eyexcam = sin(0) * 1.0 + centerx + sin(rtime) * 1.0;
					double eyezcam = cos(0) * 1.0 + centerz + cos(rtime) * 1.0;

					CEngine::cameraSET usingCamera;


					usingCamera.eyex = 3.0 + sin(rtime);
					usingCamera.eyey = 2.5;
					usingCamera.eyez = 1.0 + cos(rtime);
					usingCamera.centerx = 3.0;
					usingCamera.centery = 0.5;
					usingCamera.centerz = -5;
					usingCamera.upx = 0;
					usingCamera.upy = 1;
					usingCamera.upz = 0;
					/*usingCamera.eyex = eyexcam;
					usingCamera.eyey = 130.0;
					usingCamera.eyez = eyezcam;
					usingCamera.centerx = centerx;
					usingCamera.centery = 130.0;
					usingCamera.centerz = centerz;
					usingCamera.upx = 0;
					usingCamera.upy = 1;
					usingCamera.upz = 0;*/

					engine.setCamera(usingCamera);
				}

				{
					lightData l;

					for (auto &t : l.ambientLight)
					{
						t = 0.1f;
					}

					for (auto &t : l.direction)
					{
						t = 2.5f;
					}

					for (auto &t : l.position)
					{
						t = 0.0f;
					}

					for (auto &t : l.specularLight)
					{
						t = 0.2f;
					}

					for (auto &t : l.diffuseLight)
					{
						t = 0.2f;
					}

					l.specularLight[1] = 1.0f;
					l.specularLight[2] = 1.0f;
					l.diffuseLight[0] = 1.0f;
					l.diffuseLight[1] = 1.0f;
					l.ambientLight[3] = 0.1f;

					CEngine::colorRGBToArrayf(0xFFF6ED, l.diffuseLight);

					l.angle = 180.0;
					l.direction[0] = 3.0f;
					l.direction[1] = -0.5f;
					l.direction[2] = -1.5f;

					l.position[0] = 3.0f;
					l.position[1] = 2.7f;
					l.position[2] = -1.5f;
					l.position[3] = 1.0f;

					engine.activateLight(0, false);
					engine.activateLight(1, true);
					engine.setLight(l, 1);
				}

				/*{
				lightData l;

				for (auto &t : l.ambientLight)
				{
				t = 0.1;
				}

				for (auto &t : l.direction)
				{
				t = 2.5;
				}

				for (auto &t : l.position)
				{
				t = 0.0;
				}

				for (auto &t : l.specularLight)
				{
				t = 1.0;
				}

				for (auto &t : l.diffuseLight)
				{
				t = 0.5;
				}

				l.specularLight[1] = 1.0;
				l.specularLight[2] = 1.0;
				l.specularLight[3] = 0.5;
				l.diffuseLight[3] = 0.5;
				l.ambientLight[3] = 0.1;

				l.angle = 80.0;
				l.direction[0] = 100.0;
				l.direction[1] = -0.5;
				l.direction[2] = 0.0;

				l.position[3] = 0.0;
				l.position[1] = 50.0;
				l.position[2] = 100.5;

				engine.activateLight(0, true);
				engine.setLight(l, 0);
				}*/

				//static const unsigned int cityTextID = game.loadTexture("test", "city.tga").getTextId();
				engine.activateNormals(true);
				game.testobj.draw(0);
				engine.activateNormals(false);

				engine.activateLighting(false);
				engine.activate3DRender(false);

				/*{
					double size = 0.5;

					CEngine::RenderDoubleStruct TempStruct3D;

					static auto hopoLightText = GPPGame::GuitarPP().loadTexture("data/sprites", "hopolight.tga").getTextId();

					double flarex = 2.92, flarey = 2.54, flarez = -1.5;

					TempStruct3D.Text = hopoLightText;
					TempStruct3D.TextureX1 = 0.0;
					TempStruct3D.TextureX2 = 1.0;
					TempStruct3D.TextureY1 = 1.0;
					TempStruct3D.TextureY2 = 0.0;

					//CFonts::fonts().drawTextInScreen(std::to_string(hopostp.size()), 0.0, 0.5, 0.1);

					TempStruct3D.x1 = flarex;
					TempStruct3D.x2 = TempStruct3D.x1 + size;
					TempStruct3D.x3 = TempStruct3D.x1 + size;
					TempStruct3D.x4 = TempStruct3D.x1;

					TempStruct3D.y1 = flarey;
					TempStruct3D.y2 = TempStruct3D.y1;
					TempStruct3D.y3 = flarey + 0.2;
					TempStruct3D.y4 = TempStruct3D.y3;

					TempStruct3D.z1 = flarez + size * 2.0;
					TempStruct3D.z2 = TempStruct3D.z1;
					TempStruct3D.z3 = flarez;
					TempStruct3D.z4 = TempStruct3D.z3;

					CEngine::engine().Render3DQuad(TempStruct3D);
				}*/

				engine.matrixReset();

				engine.clear3DBuffer();

				{
					CEngine::cameraSET usingCamera;
					usingCamera.eyex = 0.0;
					usingCamera.eyey = 0.0;
					usingCamera.eyez = 2.3;
					usingCamera.centerx = 0;
					usingCamera.centery = 0;
					usingCamera.centerz = 0.0;
					usingCamera.upx = 0;
					usingCamera.upy = 1;
					usingCamera.upz = 0;

					engine.setCamera(usingCamera);
				}
			}



			module.render();

			if (!interval)
				module.renderLyrics();

			if (firstStartFrame)
			{
				engine.playSoundStream(GuitarPP().startSound);
				firstStartFrame = false;
			}

			double time = engine.getTime();

			if (musicstartedg == 0) musicstartedg = 1;

			if ((startTime - time) > 0.0)
			{

				fadeoutdsc = engine.getTime();
				
				fonts.drawTextInScreenWithBuffer(std::to_string((int)(startTime - time)), -0.3, 0.0, 0.3);
				musicstartedg = 0;
			}

			double fadeoutdscAlphaCalc = 1.0 - ((engine.getTime() - fadeoutdsc) / 3.0);

			if (fadeoutdscAlphaCalc >= 0.0)
			{
				if (fadeoutdscAlphaCalc < 1.0) engine.setColor(1.0, 1.0, 1.0, fadeoutdscAlphaCalc);

				fonts.drawTextInScreen(songName, -0.9, 0.7, 0.1);
				fonts.drawTextInScreen(songArtist, -0.88, 0.62, 0.08);
				fonts.drawTextInScreen(songCharter, -0.88, 0.54, 0.08);

				if (fadeoutdscAlphaCalc < 1.0) engine.setColor(1.0, 1.0, 1.0, 1.0);
			}

			/*{
				double deltaTol = engine.getDeltaTime();

				if (deltaTol < 0.005)
					deltaTol = 0.005;

				double rningTimeFretCalcMn = module.fretboardPositionCalcByT(-9.0 - deltaTol, 1.02);
				double rningTimeFretCalcMs = module.fretboardPositionCalcByT(-9.0 + deltaTol, 1.02);

				rningTimeFretCalcMn = rningTimeFretCalcMn < 0.0 ? -rningTimeFretCalcMn : rningTimeFretCalcMn;
				rningTimeFretCalcMs = rningTimeFretCalcMs < 0.0 ? -rningTimeFretCalcMs : rningTimeFretCalcMs;

				fonts.drawTextInScreenWithBuffer(std::to_string(rningTimeFretCalcMn), -1.3, 0.8, 0.1);
				fonts.drawTextInScreenWithBuffer(std::to_string(rningTimeFretCalcMs), -1.3, 0.65, 0.1);
				fonts.drawTextInScreenWithBuffer(std::to_string(dmaxrntime), -1.3, 0.50, 0.1);
			}*/

			if (musicstartedg == 1)
			{
				CEngine::engine().setSoundVolume(module.players.back().songAudioID, game.songVolume);
				engine.playSoundStream(module.players.back().songAudioID);

				for (auto &p : module.players)
				{
					p.instrumentPlay();
				}

				musicstartedg = 2;
			}
		}




		game.renderFrame();
		marathonTime += engine.getDeltaTime();
	}


	l.continueThread = false;
	if (load.joinable()) load.join();
}

void GPPGame::callbackRenderFrame()
{
	CFonts::fonts().drawAllBuffers();
}

void GPPGame::continueCampaing(const std::string &name)
{
	auto createMMenu = []()
	{
		auto &cntCampaing = GPPGame::GuitarPP().newNamedMenu("continueCampaingOptions");

		{
			CMenu::menuOpt opt;

			opt.text = "Patos voadores";
			opt.y = 0.3;
			opt.size = 0.075;
			opt.x = CFonts::fonts().getCenterPos(opt.text, opt.size, 0.0);
			opt.group = 1;
			opt.status = 0;
			opt.type = CMenu::menusOPT::textbtn;

			cntCampaing.addOpt(opt);
		}

		{
			CMenu::menuOpt opt;

			opt.text = "Voltar";
			opt.y = 0.0;
			opt.size = 0.075;
			opt.x = CFonts::fonts().getCenterPos(opt.text, opt.size, 0.0);
			opt.group = 1;
			opt.status = 0;
			opt.type = CMenu::menusOPT::textbtn;
			opt.goback = true;

			cntCampaing.addOpt(opt);
		}


		return &cntCampaing;
	};
	static auto &continueMenu = *createMMenu();


	auto preFun = []()
	{

		return 0;
	};

	auto midFun = []()
	{
		CFonts::fonts().drawTextInScreenWithBuffer("Seu status", 0.7, 0.5, 0.1);
		
		return 0;
	};

	auto posFun = []()
	{

		return 0;
	};

	if (CEngine::engine().windowOpened())
	{
		GPPGame::GuitarPP().clearScreen();


		GPPGame::GuitarPP().renderFrame();
	}

	GPPGame::GuitarPP().openMenus(&continueMenu, preFun, midFun, posFun);
}

void GPPGame::campaingPlayModule(const std::string &name)
{
	loadThreadData l;

	auto &game = GPPGame::GuitarPP();
	auto &engine = CEngine::engine();
	auto &fonts = CFonts::fonts();
	auto realname = game.getCallBackRealName(name);
	auto &module = game.gameModules[realname];

	if (game.getRunningModule().size() > 0)
	{
		throw gameException("A module is already running: " + name);
	}

	game.HUDText = game.loadTexture("data/sprites", "HUD.tga").getTextId();
	game.fretboardText = game.loadTexture("data/sprites", "fretboard.tga").getTextId();
	game.lineText = game.loadTexture("data/sprites", "line.tga").getTextId();
	game.HOPOSText = -1/*GPPGame::GuitarPP().loadTexture("data/sprites", "HOPOS.tga").getTextId()*/;
	game.pylmBarText = game.loadTexture("data/sprites", "pylmbar.tga").getTextId();

	module.setHyperSpeed(2.5 * game.hyperSpeed);

	game.setVSyncMode(0);

	game.setRunningModule(realname);

	module.players.push_back(CPlayer("xi"));

	l.processing = true;
	l.continueThread = true;
	l.songID = 0;
	l.songsList = getDirectory("./data/songs", false, true);
	l.listEnd = false;
	l.sendToModulePlayers = false;
	l.loadSong = true;


	for (auto &p : l.songsList)
	{
		std::cout << p << std::endl;
	}

	std::thread load(loadMarathonThread, std::ref(module), std::ref(l));

	while (engine.windowOpened() && l.processing)
	{
		game.clearScreen();

		CFonts::fonts().drawTextInScreenWithBuffer("loading marathon 0%", -0.4, 0.0, 0.1);

		game.renderFrame();
	}

	l.sendToModulePlayers = true;
	l.processing = true;

	while (engine.windowOpened() && l.processing)
	{
		game.clearScreen();

		CFonts::fonts().drawTextInScreenWithBuffer("loading marathon 50%", -0.4, 0.0, 0.1);

		game.renderFrame();
	}

	l.songID++;
	l.loadSong = true;

	for (auto &p : module.players)
	{
		p.startTime = engine.getTime() + 3.0;
		p.musicRunningTime = -3.0;
	}

	double startTime = module.players.back().startTime = engine.getTime() + 3.0;
	double openMenuTime = 0.0;
	module.players.back().musicRunningTime = -3.0;
	module.players.back().guitar = CGuitars::inst().getGuitarIfExists(game.defaultGuitar);

	if (module.players.back().guitar)
		module.players.back().guitar->load();

	bool enterInMenu = false, esc = false;
	int musicstartedg = 0;

	bool songTimeFixed = false;

	module.players.back().enableBot = game.botEnabled;
	module.players.back().playerCamera.centery = 1.0;
	module.players.back().playerCamera.eyez += 2.0;

	bool firstStartFrame = true;

	auto &playerb = module.players.back();
	std::string songName = playerb.Notes.songName, songArtist = playerb.Notes.songArtist, songCharter = playerb.Notes.songCharter;

	double fadeoutdsc = engine.getTime();

	bool interval = false, waitingIntervalLoad = false;

	while (engine.windowOpened())
	{
		game.clearScreen();

		if (engine.getKey(GLFW_KEY_ESCAPE))
		{
			esc = true;
		}
		else if (esc)
		{
			enterInMenu = true;
		}
		else
		{
			enterInMenu = false;
		}



		if (enterInMenu)
		{
			if (musicstartedg == 2)
			{
				for (auto &p : module.players)
				{
					p.instrumentPause();
				}

				engine.pauseSoundStream(module.players.back().songAudioID);
				musicstartedg = 0;
			}

			songTimeFixed = false;

			openMenuTime = engine.getTime();

			game.openMenus(&module.moduleMenu);

			if (module.moduleMenu.options[module.exitModuleOpt].status & 1)
			{
				module.resetModule();
				break;
			}

			double time = engine.getTime();

			for (auto &p : module.players)
			{
				p.startTime += time - openMenuTime;
			}

			enterInMenu = false;
			esc = false;
		}
		else
		{
			if (!interval)
				module.marathonUpdate();
			else
			{
				for (auto &p : module.players)
					p.musicRunningTime += engine.getDeltaTime() * module.getgSpeed();
			}

			bool songChartEnd = true;

			for (auto &p : module.players)
			{
				if (!interval)
				{
					if (songChartEnd)
						songChartEnd = p.isSongChartFinished();
				}

				if (p.targetCamera.centery < p.playerCamera.centery)
				{
					p.playerCamera.centery -= engine.getDeltaTime() * 0.5;
				}

				if (p.targetCamera.centery > p.playerCamera.centery)
				{
					p.playerCamera.centery = p.targetCamera.centery;
				}

				if (p.targetCamera.eyez < p.playerCamera.eyez)
				{
					p.playerCamera.eyez -= engine.getDeltaTime() * 0.75;
				}

				if (p.targetCamera.eyez > p.playerCamera.eyez)
				{
					p.playerCamera.eyez = p.targetCamera.eyez;
				}
			}

			interval = songChartEnd;

			if (interval)
			{
				if (!waitingIntervalLoad)
				{
					l.sendToModulePlayers = true;
					l.processing = true;

					waitingIntervalLoad = true;
				}
				else
				{
					if (l.processing)
					{
						//// TODO


					}
					else
					{
						double waitTime = 5.0;

						double deltaTol = engine.getDeltaTime();

						if (deltaTol < 0.005)
							deltaTol = 0.005;

						deltaTol /= 2.0;

						double dmaxrntime = 0.0;

						double rningTimeFretCalcMn = module.fretboardPositionCalcByT(module.players.back().musicRunningTime - deltaTol - 4.0, 1.02, &dmaxrntime);
						double rningTimeFretCalcMs = module.fretboardPositionCalcByT(module.players.back().musicRunningTime + deltaTol - 4.0, 1.02);

						rningTimeFretCalcMn = rningTimeFretCalcMn < 0.0 ? -rningTimeFretCalcMn : rningTimeFretCalcMn;
						rningTimeFretCalcMs = rningTimeFretCalcMs < 0.0 ? -rningTimeFretCalcMs : rningTimeFretCalcMs;

						double fretBoardPos = module.fretboardPositionCalcByT(-5 - 4.0, 1.02);

						if (fretBoardPos > 0.0)
						{
							fretBoardPos = dmaxrntime - fretBoardPos;
						}
						else
						{
							fretBoardPos *= -1.0;
						}

						if (fretBoardPos > rningTimeFretCalcMn && fretBoardPos < rningTimeFretCalcMs)
						{
							for (auto &p : module.players)
							{
								p.startTime = engine.getTime() + waitTime;
								p.musicRunningTime = -waitTime;
							}

							startTime = module.players.back().startTime = engine.getTime() + waitTime;
							openMenuTime = 0.0;
							module.players.back().musicRunningTime = -waitTime;

							songTimeFixed = false;
							musicstartedg = 0;

							l.songID++;
							interval = false;
							l.loadSong = true;
							waitingIntervalLoad = false;

							auto &playerb = module.players.back();
							songName = playerb.Notes.songName;
							songArtist = playerb.Notes.songArtist;
							songCharter = playerb.Notes.songCharter;
						}
					}
				}
			}
			else
			{
				if (!songTimeFixed && (engine.getTime() - startTime) > 0.5)
				{
					engine.setSoundTime(module.players.back().songAudioID, module.players.back().musicRunningTime);

					for (auto &p : module.players)
					{
						engine.setSoundTime(p.instrumentSound, p.musicRunningTime);
					}

					songTimeFixed = true;
				}
			}


			{


				engine.activate3DRender(true);
				engine.activateLighting(true);

				{
					double centerx = 0.0;
					double centerz = -650.0;

					double rtime = module.players.back().musicRunningTime / 10.0;
					double eyexcam = sin(0) * 1.0 + centerx + sin(rtime) * 1.0;
					double eyezcam = cos(0) * 1.0 + centerz + cos(rtime) * 1.0;

					CEngine::cameraSET usingCamera;


					usingCamera.eyex = 3.0 + sin(rtime);
					usingCamera.eyey = 2.5;
					usingCamera.eyez = 1.0 + cos(rtime);
					usingCamera.centerx = 3.0;
					usingCamera.centery = 0.5;
					usingCamera.centerz = -5;
					usingCamera.upx = 0;
					usingCamera.upy = 1;
					usingCamera.upz = 0;

					engine.setCamera(usingCamera);
				}

				{
					lightData l;

					for (auto &t : l.ambientLight)
					{
						t = 0.1f;
					}

					for (auto &t : l.direction)
					{
						t = 2.5f;
					}

					for (auto &t : l.position)
					{
						t = 0.0f;
					}

					for (auto &t : l.specularLight)
					{
						t = 0.2f;
					}

					for (auto &t : l.diffuseLight)
					{
						t = 0.2f;
					}

					l.specularLight[1] = 1.0f;
					l.specularLight[2] = 1.0f;
					l.diffuseLight[0] = 1.0f;
					l.diffuseLight[1] = 1.0f;
					l.ambientLight[3] = 0.1f;

					CEngine::colorRGBToArrayf(0xFFF6ED, l.diffuseLight);

					l.angle = 180.0f;
					l.direction[0] = 3.0f;
					l.direction[1] = -0.5f;
					l.direction[2] = -1.5f;

					l.position[0] = 3.0f;
					l.position[1] = 2.7f;
					l.position[2] = -1.5f;
					l.position[3] = 1.0f;

					engine.activateLight(0, false);
					engine.activateLight(1, true);
					engine.setLight(l, 1);
				}

				engine.activateNormals(true);
				game.testobj.draw(0);
				engine.activateNormals(false);

				engine.activateLighting(false);
				engine.activate3DRender(false);

				engine.matrixReset();

				engine.clear3DBuffer();

				{
					CEngine::cameraSET usingCamera;
					usingCamera.eyex = 0.0;
					usingCamera.eyey = 0.0;
					usingCamera.eyez = 2.3;
					usingCamera.centerx = 0;
					usingCamera.centery = 0;
					usingCamera.centerz = 0.0;
					usingCamera.upx = 0;
					usingCamera.upy = 1;
					usingCamera.upz = 0;

					engine.setCamera(usingCamera);
				}
			}



			module.render();

			if (!interval)
				module.renderLyrics();

			if (firstStartFrame)
			{
				engine.playSoundStream(GuitarPP().startSound);
				firstStartFrame = false;
			}

			double time = engine.getTime();

			if (musicstartedg == 0) musicstartedg = 1;

			if ((startTime - time) > 0.0)
			{

				fadeoutdsc = engine.getTime();

				fonts.drawTextInScreenWithBuffer(std::to_string((int)(startTime - time)), -0.3, 0.0, 0.3);
				musicstartedg = 0;
			}

			double fadeoutdscAlphaCalc = 1.0 - ((engine.getTime() - fadeoutdsc) / 3.0);

			if (fadeoutdscAlphaCalc >= 0.0)
			{
				if (fadeoutdscAlphaCalc < 1.0) engine.setColor(1.0, 1.0, 1.0, fadeoutdscAlphaCalc);

				fonts.drawTextInScreen(songName, -0.9, 0.7, 0.1);
				fonts.drawTextInScreen(songArtist, -0.88, 0.62, 0.08);
				fonts.drawTextInScreen(songCharter, -0.88, 0.54, 0.08);

				if (fadeoutdscAlphaCalc < 1.0) engine.setColor(1.0, 1.0, 1.0, 1.0);
			}

			if (musicstartedg == 1)
			{
				CEngine::engine().setSoundVolume(module.players.back().songAudioID, game.songVolume);
				engine.playSoundStream(module.players.back().songAudioID);

				for (auto &p : module.players)
				{
					p.instrumentPlay();
				}

				musicstartedg = 2;
			}
		}




		game.renderFrame();
	}


	l.continueThread = false;
	if (load.joinable()) load.join();
}

void GPPGame::eraseGameMenusAutoCreateds()
{
	for (auto it = gameMenus.begin(); it != gameMenus.end(); /******/)
	{
		if ((*it).second.gameMenu)
		{
			++it;
		}
		else
		{
			it = gameMenus.erase(it);
		}
	}
}

CMenu &GPPGame::newMenu()
{
	CMenu m;
	gameMenus[m.getName()] = std::move(m);
	gameMenus[m.getName()].gameMenu = true;
	return gameMenus[m.getName()];
}

CMenu &GPPGame::newNamedMenu(const std::string &name)
{
	CMenu m(name);
	gameMenus[m.getName()] = std::move(m);
	gameMenus[m.getName()].gameMenu = true;
	return gameMenus[m.getName()];
}

void GPPGame::settWindowConfigs(const gameWindow &w)
{
	windowCFGs = w;
}

/*
Clear screen buffer
*/
void GPPGame::clearScreen()
{
	CLuaH::Lua().runEvent("preClearScreen");
	CEngine::engine().clearScreen();
	CLuaH::Lua().runEvent("posClearScreen");
}

/*
Swap video buffer to screen
*/
void GPPGame::renderFrame()
{
	CLuaH::Lua().runEvent("preRenderFrame");
	CEngine::engine().renderFrame();
	CLuaH::Lua().runEvent("posRenderFrame");
}

GPPGame &GPPGame::GuitarPP()
{
	static GPPGame game;
	return game;
}

double GPPGame::getWindowProportion()
{
	return CEngine::engine().windowWidth / CEngine::engine().windowHeight;
}

const GPPGame::gppTexture &GPPGame::loadTexture(const std::string &path, const std::string &texture, CLuaH::luaScript *luaScript)
{
	auto &textInst = gTextures[(path + "/" + texture)];

	if (luaScript)
	{
		textInst.associatedToScript[luaScript] = true;
	}

	if (textInst.getTextId())
	{
		return gTextures[(path + "/" + texture)];
	}

	gTextures[(path + "/" + texture)] = std::move(gppTexture(path, texture));
	return gTextures[(path + "/" + texture)];
}

void GPPGame::loadBasicSprites()
{
	CLuaH::Lua().runEvent("preLoadSprites");
	SPR["palheta"] = CEngine::engine().loadTexture("data/sprites/palheta.tga");

}

bool GPPGame::CTheme::load()
{
	const std::string path = std::string("data/themes/") + themeName;

	if (themeName.size() == 0){
		CLog::log() << "GPPGame::CTheme::load(): Load theme fail - no name setted";
		return false;
	}

	//main = CLuaH::Lua().newScriptInQuere(CLuaH::Lua().newScript(path, "Theme.lua"));
	CLuaH::Lua().loadFiles(path);
	
	if (CLuaH::Lua().getScript(path, "Theme.lua").luaState == nullptr){
		CLog::log() << "GPPGame::CTheme::load(): Load theme fail - Theme.lua wasn't loaded";
		return false;
	}

	CLuaH::Lua().runScript(path, "Theme.lua");

	loaded = true;

	return true;
}

const GPPGame::CTheme &GPPGame::loadThemes(const std::string &theme, CLuaH::luaScript *luaScript)
{
	auto &themeInst = gThemes[theme];
	themeInst.themeName = theme;

	if (!themeInst.isloaded())
	{
		themeInst.load();
	}
	
	return gThemes[theme];
}

void GPPGame::loadAllThemes()
{
	const std::string path = "data/themes";

	auto extension_from_filename = [](const std::string &fname)
	{
		size_t s;
		return std::string(((s = fname.find_first_of('.')) != fname.npos) ? (&fname.c_str()[++s]) : (""));
	};

	auto file_exists = [](const std::string &fileName)
	{
		return std::fstream(fileName).is_open();
	};

	DIR *direntd = opendir((std::string("./") + path).c_str());
	dirent *rrd = nullptr;

	if (direntd)
	{
		rrd = readdir(direntd);
		while ((rrd = readdir(direntd)) != nullptr)
		{
			const std::string name = rrd->d_name;

			if ((rrd->d_type & DT_DIR) && name != ".." && name != "."
				&& file_exists((path + std::string("/") + name + "/Theme.lua")))
			{
				loadThemes(name);
			}
		}
		closedir(direntd);
	}
	/*
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{

			if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				
			}
		} while (FindNextFile(hFind, &data));
		FindClose(hFind);
	}
	*/
}

const std::string GPPGame::addGameCallbacks(const std::string &n, func_t function)
{
	std::string str = std::string("game_callback_") + std::to_string(rand()) + "_" + n;

	gameCallbacks[str] = function;
	gameCallbacksWrapper[str] = n;

	return str;
}

std::string GPPGame::getCallBackRealName(const std::string &str)
{
	for (auto &s : gameCallbacksWrapper)
	{
		if (s.second == str)
		{
			return s.first;
		}
	}
	return "";
}

std::string GPPGame::selectSong()
{
	auto &game = GuitarPP();
	static auto &selectSongMenu = GuitarPP().newNamedMenu("selectSongMenu");
	static bool selectingSong = false;
	std::map<int, std::string> menuMusics;

	menuMusics.clear();
	selectSongMenu.options.clear();

	auto wait = [this](double t) {
		auto &engine = CEngine::engine();
		double start = engine.getTime();

		while (engine.windowOpened() && (engine.getTime() - start) < t) {
			engine.clearScreen();

			engine.activate3DRender(true);
			engine.activateLighting(true);

			{
				CEngine::cameraSET usingCamera;
				usingCamera.eyex = 3.0;
				usingCamera.eyey = 1.75;
				usingCamera.eyez = 2.7;
				usingCamera.centerx = 3.0;
				usingCamera.centery = 1.3;
				usingCamera.centerz = 0;
				usingCamera.upx = 0;
				usingCamera.upy = 1;
				usingCamera.upz = 0;

				engine.setCamera(usingCamera);
			}

			{
				lightData l;

				for (auto &t : l.ambientLight)
				{
					t = 0.1f;
				}

				for (auto &t : l.direction)
				{
					t = 2.5f;
				}

				for (auto &t : l.position)
				{
					t = 0.0f;
				}

				for (auto &t : l.specularLight)
				{
					t = 1.0f;
				}

				for (auto &t : l.diffuseLight)
				{
					t = 0.2f;
				}

				l.specularLight[1] = 1.0f;
				l.specularLight[2] = 1.0f;
				l.diffuseLight[0] = 1.0f;
				l.diffuseLight[1] = 1.0f;
				l.ambientLight[3] = 0.1f;

				CEngine::colorRGBToArrayf(0xFFF6ED, l.diffuseLight);

				l.angle = 180.0f;
				l.direction[0] = 3.0f;
				l.direction[1] = -0.5f;
				l.direction[2] = -1.5f;

				l.position[0] = 3.0f;
				l.position[1] = 2.7f;
				l.position[2] = -1.5f;
				l.position[3] = 1.0f;

				engine.activateLight(0, false);
				engine.activateLight(1, true);
				engine.setLight(l, 1);
			}

			engine.activateNormals(true);
			testobj.draw(0);
			engine.activateNormals(false);

			engine.activateLighting(false);
			engine.activate3DRender(false);

			engine.renderFrame();
		}
	};

	int voltarOpt;

	{
		CMenu::menuOpt opt;

		opt.text = "Voltar";
		opt.y = 0.85;
		opt.x = -0.8;
		opt.size = 0.1;
		opt.group = -1;
		opt.status = 0;
		opt.type = CMenu::menusOPT::textbtn;
		opt.goback = true;

		opt.shortcutKey = GLFW_KEY_ESCAPE;

		opt.color[0] = opt.color[1] = opt.color[2] = 0.5;

		opt.updateCppCallback = [&menuMusics](CMenu &menu, CMenu::menuOpt &opt)
		{
			static double offset = 0.0;

			if (selectingSong)
			{
				int upkey = CEngine::engine().getKey(GLFW_KEY_UP), downkey = CEngine::engine().getKey(GLFW_KEY_DOWN);

				if ((upkey || downkey) && upkey != downkey)
				{
					static double movetime = CEngine::engine().getTime();

					if ((CEngine::engine().getTime() - movetime) > 0.1)
					{
						double loffset = 0.0;

						if (upkey && offset > -1.0)
						{
							loffset -= 0.1;
							movetime = CEngine::engine().getTime();
						}

						if (downkey && offset < 1.0)
						{
							loffset += 0.1;
							movetime = CEngine::engine().getTime();
						}

						offset += loffset;

						for (auto &musicOpt : menuMusics)
						{
							if (musicOpt.first < menu.options.size())
							{
								menu.options[musicOpt.first].y += loffset;
							}
						}
					}
				}
			}
			else
			{
				offset = 0;
			}
			return 0;
		};

		voltarOpt = selectSongMenu.addOpt(opt);
	}

	{
		std::atomic<bool> processingLoadDir = true;
		std::mutex getDirLoadMutex;
		std::thread getDirLoad([&]()
		{
			std::lock_guard<std::mutex> m(getDirLoadMutex);
			auto songs = game.getDirectory("./data/songs", false, true);

			for (auto &song : songs)
			{
				if (CPlayer::smartChartSearch(song) != "")
				{
					CMenu::menuOpt opt;

					opt.text = song;
					opt.y = 0.8 - menuMusics.size() * 0.1;
					opt.x = CFonts::fonts().getCenterPos(song, 0.09, 0.0);
					opt.size = 0.09;
					opt.group = 1;
					opt.status = 0;
					opt.type = CMenu::menusOPT::textbtn;
					opt.goback = true;

					opt.color[1] = opt.color[2] = (CPlayer::smartSongSearch(song).size() == 0)? 0.0 : 1.0;

					menuMusics[selectSongMenu.addOpt(opt)] = song;
				}
			}

			processingLoadDir = false;
		});

		bool mutexLocked = false;

		auto mutexTryLockTestGuard = [&]()
		{
			if (!mutexLocked)
			{
				return (mutexLocked = getDirLoadMutex.try_lock());
			}

			return mutexLocked;
		};

		while (processingLoadDir || !mutexTryLockTestGuard())
		{
			if (!CEngine::engine().windowOpened())
			{
				if (!mutexLocked)
				{
					std::lock_guard<std::mutex> m(getDirLoadMutex);

					if (getDirLoad.joinable())
						getDirLoad.join();
				}
				else
				{
					try {
						if (getDirLoad.joinable())
							getDirLoad.join();
					}
					catch (...) { }

					getDirLoadMutex.unlock();
				}

				mutexLocked = false;
				return std::string();
			}

			wait(0.1);
		}

		if (mutexLocked)
		{
			getDirLoadMutex.unlock();

			if (getDirLoad.joinable())
				getDirLoad.join();
		}
	}

	while ((CEngine::engine().getMouseButton(0) || CEngine::engine().getKey(GLFW_KEY_ENTER)))
	{
		if (!CEngine::engine().windowOpened())
		{
			return std::string();
		}

		wait(0.1);
	}

	selectingSong = true;

	openMenus(&selectSongMenu);

	selectingSong = false;

	auto &voltarOptInst = selectSongMenu.options[voltarOpt];
	voltarOptInst.updateCppCallback(selectSongMenu, voltarOptInst);

	if ((selectSongMenu.options[voltarOpt].status & 3) == 3) {
		return std::string();
	}
	else if (selectSongMenu.groupInfo[1].selectedOpt != 0) {
		for (auto &musicOpt : menuMusics) {
			if ((selectSongMenu.options[musicOpt.first].status & 3) == 3) {
				return musicOpt.second;
			}
		}
	}

	return std::string();
}

void GPPGame::addSongListToMenu(CMenu &selectSongMenu, std::map<int, std::string> &menuMusics)
{
	auto &game = GuitarPP();
	static bool selectingSong = false;

	menuMusics.clear();
	selectSongMenu.options.clear();

	auto wait = [this](double t) {
		auto &engine = CEngine::engine();
		double start = engine.getTime();

		while (engine.windowOpened() && (engine.getTime() - start) < t) {
			engine.clearScreen();

			engine.activate3DRender(true);
			engine.activateLighting(true);

			{
				CEngine::cameraSET usingCamera;
				usingCamera.eyex = 3.0;
				usingCamera.eyey = 1.75;
				usingCamera.eyez = 2.7;
				usingCamera.centerx = 3.0;
				usingCamera.centery = 1.3;
				usingCamera.centerz = 0;
				usingCamera.upx = 0;
				usingCamera.upy = 1;
				usingCamera.upz = 0;

				engine.setCamera(usingCamera);
			}

			{
				lightData l;

				for (auto &t : l.ambientLight)
				{
					t = 0.1f;
				}

				for (auto &t : l.direction)
				{
					t = 2.5f;
				}

				for (auto &t : l.position)
				{
					t = 0.0f;
				}

				for (auto &t : l.specularLight)
				{
					t = 1.0f;
				}

				for (auto &t : l.diffuseLight)
				{
					t = 0.2f;
				}

				l.specularLight[1] = 1.0f;
				l.specularLight[2] = 1.0f;
				l.diffuseLight[0] = 1.0f;
				l.diffuseLight[1] = 1.0f;
				l.ambientLight[3] = 0.1f;

				CEngine::colorRGBToArrayf(0xFFF6ED, l.diffuseLight);

				l.angle = 180.0f;
				l.direction[0] = 3.0f;
				l.direction[1] = -0.5f;
				l.direction[2] = -1.5f;

				l.position[0] = 3.0f;
				l.position[1] = 2.7f;
				l.position[2] = -1.5f;
				l.position[3] = 1.0f;

				engine.activateLight(0, false);
				engine.activateLight(1, true);
				engine.setLight(l, 1);
			}

			engine.activateNormals(true);
			testobj.draw(0);
			engine.activateNormals(false);

			engine.activateLighting(false);
			engine.activate3DRender(false);

			engine.renderFrame();
		}
	};

	int voltarOpt;

	{
		CMenu::menuOpt opt;

		opt.text = "Voltar";
		opt.y = 0.85;
		opt.x = -0.8;
		opt.size = 0.1;
		opt.group = -1;
		opt.status = 0;
		opt.type = CMenu::menusOPT::textbtn;
		opt.goback = true;

		opt.shortcutKey = GLFW_KEY_ESCAPE;

		opt.color[0] = opt.color[1] = opt.color[2] = 0.5;

		opt.updateCppCallback = [&menuMusics](CMenu &menu, CMenu::menuOpt &opt)
		{
			static double offset = 0.0;

			if (selectingSong)
			{
				int upkey = CEngine::engine().getKey(GLFW_KEY_UP), downkey = CEngine::engine().getKey(GLFW_KEY_DOWN);

				if ((upkey || downkey) && upkey != downkey)
				{
					static double movetime = CEngine::engine().getTime();

					if ((CEngine::engine().getTime() - movetime) > 0.1)
					{
						double loffset = 0.0;

						if (upkey && offset > -1.0)
						{
							loffset -= 0.1;
							movetime = CEngine::engine().getTime();
						}

						if (downkey && offset < 1.0)
						{
							loffset += 0.1;
							movetime = CEngine::engine().getTime();
						}

						offset += loffset;

						for (auto &musicOpt : menuMusics)
						{
							if (musicOpt.first < menu.options.size())
							{
								menu.options[musicOpt.first].y += loffset;
							}
						}
					}
				}
			}
			else
			{
				offset = 0;
			}
			return 0;
		};

		voltarOpt = selectSongMenu.addOpt(opt);
	}

	{
		std::atomic<bool> processingLoadDir = true;
		std::mutex getDirLoadMutex;
		std::thread getDirLoad([&]()
		{
			std::lock_guard<std::mutex> m(getDirLoadMutex);
			auto songs = game.getDirectory("./data/songs", false, true);

			for (auto &song : songs)
			{
				if (CPlayer::smartChartSearch(song) != "")
				{
					CMenu::menuOpt opt;

					opt.text = song;
					opt.y = 0.8 - menuMusics.size() * 0.1;
					opt.x = CFonts::fonts().getCenterPos(song, 0.09, 0.0);
					opt.size = 0.09;
					opt.group = 1;
					opt.status = 0;
					opt.type = CMenu::menusOPT::textbtn;

					opt.color[1] = opt.color[2] = (CPlayer::smartSongSearch(song).size() == 0) ? 0.0 : 1.0;

					menuMusics[selectSongMenu.addOpt(opt)] = song;
				}
			}

			processingLoadDir = false;
		});

		bool mutexLocked = false;

		auto mutexTryLockTestGuard = [&]()
		{
			if (!mutexLocked)
			{
				return (mutexLocked = getDirLoadMutex.try_lock());
			}

			return mutexLocked;
		};

		while (processingLoadDir || !mutexTryLockTestGuard())
		{
			if (!CEngine::engine().windowOpened())
			{
				if (!mutexLocked)
				{
					std::lock_guard<std::mutex> m(getDirLoadMutex);

					if (getDirLoad.joinable())
						getDirLoad.join();
				}
				else
				{
					try {
						if (getDirLoad.joinable())
							getDirLoad.join();
					}
					catch (...) {}

					getDirLoadMutex.unlock();
				}

				mutexLocked = false;
				return;
			}

			wait(0.1);
		}

		if (mutexLocked)
		{
			getDirLoadMutex.unlock();

			if (getDirLoad.joinable())
				getDirLoad.join();
		}
	}
}

GPPGame::func_t GPPGame::getCallback(const std::string &str)
{
	return gameCallbacks[str];
}

std::string GPPGame::getRunningModule()
{
	return runningModule;
}

std::deque <CMenu*> GPPGame::openMenus(CMenu *startMenu, std::function<int(void)> preFun, std::function<int(void)> midFun, std::function<int(void)> posFun, bool dev, std::deque < CMenu* > stackTest)
{
	callOnDctor<void(void)> exitguard([]()
	{
		GPPGame::GuitarPP().currentMenu = nullptr;
	});

	auto &engine = CEngine::engine();
	auto &lua = CLuaH::Lua();
	double waitForTime = 0.0;
	
	std::deque < CMenu* > menusStack = stackTest;
	//static std::deque < CMenu* > devMenusStack;

	if (menusStack.size() == 0)
	{
		menusStack.push_back(startMenu);

		currentMenu = startMenu;
	}
	else
	{
		currentMenu = menusStack.back();
	}
	//auto vertexbuffer = engine.vboSET(vertices.size() * sizeof(glm::vec3), &vertices[0]);
	//auto uvbuffer = engine.vboSET(uvs.size() * sizeof(glm::vec2), &uvs[0]);


	auto create_menu = [&](const std::deque < std::string > &menusXRef)
	{
		auto &menu = newMenu();

		menu.temp = true;

		int optn = 0;

		for (auto &m : menusXRef)
		{
			CMenu::menuOpt opt;

			opt.text = m;
			opt.y = 0.5 - optn * 0.2;
			opt.x = 0.15;
			opt.size = 0.075;
			opt.group = 1;
			opt.status = 0;
			opt.type = CMenu::menusOPT::textbtn;
			opt.menusXRef.push_back(m);

			menu.addOpt(opt);

			++optn;
		}
		
		CMenu::menuOpt opt;

		opt.text = "Back/voltar";
		opt.y = -0.75;
		opt.x = 0.5;
		opt.size = 0.075;
		opt.group = 1;
		opt.status = 0;
		opt.type = CMenu::menusOPT::textbtn;
		opt.goback = true;

		menu.addOpt(opt);

		return &menu;
	};

	CEngine::RenderDoubleStruct RenderData;

	RenderData.y1 = 1.0;
	RenderData.y2 = 1.0;
	RenderData.y3 = -1.0;
	RenderData.y4 = -1.0;

	RenderData.z1 = 0.0;
	RenderData.z2 = 0.0;
	RenderData.z3 = 0.0;
	RenderData.z4 = 0.0;

	RenderData.TextureX1 = 0.0;
	RenderData.TextureX2 = 1.0;
	RenderData.TextureY1 = 1.0;
	RenderData.TextureY2 = 0.0;

	bool back = false;
	bool updateRender = true;

	bool menuExit = false;

	bool waitReleaseKeys = false;

	while (menusStack.size() != 0 && engine.windowOpened())
	{
		clearScreen();

		if (menuExit)
		{
			currentMenu = nullptr;
			return menusStack;
		}

		if (waitReleaseKeys)
		{
			if (!CEngine::engine().getMouseButton(0) && !CEngine::engine().getKey(GLFW_KEY_ENTER) && !CEngine::engine().getKey(GLFW_KEY_ESCAPE))
			{
				waitReleaseKeys = false;
			}
		}

		updateRender = ((engine.getTime() - waitForTime) >= 0.0) && (!waitReleaseKeys);

		engine.activate3DRender(false);

		auto &menu = *menusStack.back();
		currentMenu = &menu;

		if (dev)
		{
			devMenus.devEditMenu = &menu;
		}

		if (preFun)
		{
			int preFunResult = preFun();

			switch (preFunResult)
			{
			case 0:
				break;

			case 1:
				menuExit = true;
				break;

			default:
				break;
			}
		}

		if (dev)
		{
			devMenus.update();

			for (auto &opt : devMenus.options)
			{
				if ((opt.status & 3) == 3)
				{
					if (opt.menusXRef.size() > 0)
					{
						try{
							auto function = getCallback(opt.menusXRef[0]);
							if (function)
							{
								function(opt.menusXRef[0]);
							}
						}
						catch (const std::exception &e){
							CLog::log() << e.what();
						}
					}
				}
			}
		}

		engine.activate3DRender(true);
		engine.activateLighting(true);

		{
			CEngine::cameraSET usingCamera;
			usingCamera.eyex = 3.0;
			usingCamera.eyey = 1.75;
			usingCamera.eyez = 2.7;
			usingCamera.centerx = 3.0;
			usingCamera.centery = 1.3;
			usingCamera.centerz = 0;
			usingCamera.upx = 0;
			usingCamera.upy = 1;
			usingCamera.upz = 0;

			engine.setCamera(usingCamera);
		}

		{
			lightData l;

			for (auto &t : l.ambientLight)
			{
				t = 0.1f;
			}

			for (auto &t : l.direction)
			{
				t = 2.5f;
			}

			for (auto &t : l.position)
			{
				t = 0.0f;
			}

			for (auto &t : l.specularLight)
			{
				t = 1.0f;
			}

			for (auto &t : l.diffuseLight)
			{
				t = 0.2f;
			}

			l.specularLight[1] = 1.0f;
			l.specularLight[2] = 1.0f;
			l.diffuseLight[0] = 1.0f;
			l.diffuseLight[1] = 1.0f;
			l.ambientLight[3] = 0.1f;

			CEngine::colorRGBToArrayf(0xFFF6ED, l.diffuseLight);

			l.angle = 180.0f;
			l.direction[0] = 3.0f;
			l.direction[1] = -0.5f;
			l.direction[2] = -1.5f;

			l.position[0] = 3.0f;
			l.position[1] = 2.7f;
			l.position[2] = -1.5f;
			l.position[3] = 1.0f;

			engine.activateLight(0, false);
			engine.activateLight(1, true);
			engine.setLight(l, 1);
		}

		engine.activateNormals(true);
		testobj.draw(0);
		engine.activateNormals(false);

		engine.activateLighting(false);
		engine.activate3DRender(false);

		/*{
			double size = 0.4;

			CEngine::RenderDoubleStruct TempStruct3D;

			static auto hopoLightText = GPPGame::GuitarPP().loadTexture("data/sprites", "hopolight.tga").getTextId();

			double flarex = 2.93, flarey = 2.15, flarez = -1.0;

			TempStruct3D.Text = hopoLightText;
			TempStruct3D.TextureX1 = 0.0;
			TempStruct3D.TextureX2 = 1.0;
			TempStruct3D.TextureY1 = 1.0;
			TempStruct3D.TextureY2 = 0.0;

			//CFonts::fonts().drawTextInScreen(std::to_string(hopostp.size()), 0.0, 0.5, 0.1);

			TempStruct3D.x1 = flarex;
			TempStruct3D.x2 = TempStruct3D.x1 + size;
			TempStruct3D.x3 = TempStruct3D.x1 + size;
			TempStruct3D.x4 = TempStruct3D.x1;

			TempStruct3D.y1 = flarey;
			TempStruct3D.y2 = TempStruct3D.y1;
			TempStruct3D.y3 = flarey + 0.2;
			TempStruct3D.y4 = TempStruct3D.y3;

			TempStruct3D.z1 = flarez + size * 2.0;
			TempStruct3D.z2 = TempStruct3D.z1;
			TempStruct3D.z3 = flarez;
			TempStruct3D.z4 = TempStruct3D.z3;

			CEngine::engine().Render3DQuad(TempStruct3D);
		}*/

		engine.matrixReset();

		{
			CEngine::cameraSET usingCamera;
			usingCamera.eyex = 0.0;
			usingCamera.eyey = 0.0;
			usingCamera.eyez = 2.3;
			usingCamera.centerx = 0;
			usingCamera.centery = 0;
			usingCamera.centerz = 0.0;
			usingCamera.upx = 0;
			usingCamera.upy = 1;
			usingCamera.upz = 0;

			engine.setCamera(usingCamera);
		}

		if (updateRender)
		{
			if (dev)
			{
				menu.updateDev();
			}
			else
				menu.update();
		}

		auto &texture = gTextures[menu.backgroundTexture];

		if ((RenderData.Text = texture.getTextId()))
		{
			double prop = (double)texture.getImgWidth() / (double)texture.getImgHeight();

			RenderData.x1 = -prop;
			RenderData.x2 = prop;
			RenderData.x3 = prop;
			RenderData.x4 = -prop;

			engine.Render2DQuad(RenderData);
		}

		if (midFun)
		{
			int midFunResult = midFun();

			switch (midFunResult)
			{
			case 0:
				break;

			case 1:
				menuExit = true;
				break;

			default:
				break;
			}
		}

		menu.render();

		if (dev)
		{
			devMenus.render();
			CMenu::renderUiList();

			//if (devMenusStack.size() > 0)
			//	(*devMenusStack.back()).render();
		}

		for (auto &opt : menu.options)
		{
			if ((opt.status & 3) == 3)
			{
				if (opt.goback)
				{
					// Clean a temporary menu

					if (menu.temp)
					{
						//std::cout << "size bef erase" << menusStack.size() << std::endl;
						gameMenus.erase(menu.getName());
						//std::cout << "size aft erase" << menusStack.size() << std::endl;
					}

					menusStack.pop_back();

					if (menusStack.size())
					{
						CMenu *mback = menusStack.back();

						if (mback)
							mback->resetData();
					}

					waitForTime = engine.getTime() + 0.5;

					lua.runEvent("menusGoBack");
					break;
				}

				if (opt.menusXRef.size() > 1)
				{
					menusStack.push_back(create_menu(opt.menusXRef));
					lua.runEvent("menusCustomMultiMenu");
					lua.runEvent("menusNext");
					waitForTime = engine.getTime() + 0.5;
					waitReleaseKeys = true;
					break;
				}
				else if (opt.menusXRef.size() == 1)
				{
					menusStack.push_back(&getMenuByName(opt.menusXRef[0]));
					auto &m = menusStack.back();

					if (m && !m->gameMenu)
					{
						try{
							auto function = getCallback(opt.menusXRef[0]);
							if (function)
							{
								lua.runEvent("menusGameCallbackNext");
								function(opt.menusXRef[0]);
							}
							else
							{
								CLog::log() << (opt.menusXRef[0] + " is null");
							}
						}
						catch (const std::exception &e){
							CLog::log() << e.what();
							CLuaH::multiCallBackParams_t param;

							param.push_back(e.what());

							lua.runEventWithParams("catchedException", param);
						}

						menusStack.pop_back();

						if (menusStack.size() > 0)
						{
							auto &menum = menusStack.back();
							menum->resetBtns();
							menum->resetData();
						}
					}
					else{
						if (m && m->openCallback)
						{
							m->openCallback(*m);
						}

						if (m)
						{
							m->resetBtns();
							m->resetData();
							waitReleaseKeys = true;
						}

						lua.runEvent("menusNext");
					}

					waitForTime = engine.getTime() + 0.01;
					break;
				}
			}
		}

		eraseGameMenusAutoCreateds();


		if (posFun)
		{
			int posFunResult = posFun();

			switch (posFunResult)
			{
			case 0:
				break;

			case 1:
				menuExit = true;
				break;

			default:
				break;
			}
		}

		//engine.bindTextOnSlot(0, 0);

		//engine.attribVBOBuff(0, 3, vertexbuffer);
		//engine.attribVBOBuff(1, 2, uvbuffer);

		//engine.RenderCustomVericesFloat(&vertices[0], &uvs[0], vertices.size());

		//engine.disableBuf(0);
		//engine.disableBuf(1);

		const char prog[] = { "Programado por Fabio Rossini Sluzala" };
		const char brmods[] = { "http://brmodstudio.forumeiros.com/" };
		const char oneByte[] = { "http://2nibble.forumeiros.com/" };
		
		/*
		CFonts::fonts().drawTextInScreen(prog, CFonts::fonts().getCenterPos(strlen(prog), 0.08, 0.0), -0.8, 0.08);

		CEngine::engine().setColor(0.5, 0.5, 1.0, 1.0);

		CFonts::fonts().drawTextInScreen(brmods, CFonts::fonts().getCenterPos(strlen(brmods), 0.05, 0.0), -0.87, 0.05);
		CFonts::fonts().drawTextInScreen(oneByte, CFonts::fonts().getCenterPos(strlen(oneByte), 0.05, 0.0), -0.93, 0.05);
		*/
		CEngine::engine().setColor(1.0, 1.0, 1.0, 1.0);

		GPPGame::GuitarPP().renderFrame();
	}

	menusStack.clear();

	currentMenu = nullptr;

	return std::deque<CMenu*>();
}

void GPPGame::teste(const std::string &name)
{
	std::cout << name << std::endl;;
}

void GPPGame::CTheme::apply()
{
	const std::string path = std::string("data/themes/") + themeName;

	CLuaH::Lua().runInternalEvent(CLuaH::Lua().getScript(path, "Theme.lua"), "applyTheme");
}

GPPGame::CTheme::CTheme(const std::string &theme)
{
	themeName = theme;

	loaded = true;

}

GPPGame::CTheme::CTheme()
{
	loaded = false;

}

/*
Lua events and creates window
*/
int GPPGame::createWindow()
{
	CLuaH::Lua().runEvent("preCreateWindow");
	std::string title = "Guitar++";

	// Custom title
	if (getWindowConfig().name.size() > 0){
		title += " - ";
		title += getWindowConfig().name;
	}

	// GLFW SAMPLES - Anti aliasing
	CEngine::engine().AASamples = getWindowConfig().AA;

	CEngine::engine().openWindow(title.c_str(), getWindowConfig().w, getWindowConfig().h, getWindowConfig().fullscreen);

	if (getWindowConfig().VSyncMode >= 0 && getWindowConfig().VSyncMode <= 2)
	{
		setVSyncMode(getWindowConfig().VSyncMode);
	}

	try
	{
		/*CShader::inst();
		CShader::inst().addEvent("test");
		int l0 = CShader::inst().newShader("vert.vert", 0, "test");
		int l = CShader::inst().newShader("frag.frag", 1, "test");
		CShader::inst().addShaderToEvent("test", l0);
		CShader::inst().addShaderToEvent("test", l);
		CShader::inst().linkAllShaders();*/
	}
	catch (std::exception &e)
	{
		CLog::log() << e.what();
	}


	CLuaH::Lua().runEvent("posCreateWindow");

	return CEngine::engine().windowOpened();
}

void GPPGame::setVSyncMode(int mode)
{
	CEngine::engine().setVSyncMode(mode);
}

void GPPGame::setMainMenu(CMenu &m)
{
	mainMenu = &m;
}

CMenu *GPPGame::getMainMenu()
{
	return mainMenu;
}

void GPPGame::logError(int code, const std::string &e)
{
	CLog::log() << std::to_string(code) + ": " + e;
}

std::string GPPGame::ip = "127.0.0.1";
std::string GPPGame::port = "7777";

int GPPGame::registerFunctions(CLuaH::luaState &Lstate)
{
	lua_State *L = Lstate.get();
	
	lua_register(L, "loadSingleTexture", loadSingleTexture);
	lua_register(L, "getGameplayRunningTime", getGameplayRunningTime);
	lua_register(L, "getDeltaTime", getDeltaTime);
	lua_register(L, "getGamePlayPlusState", getGamePlayPlusState);

	return 0;
}

int GPPGame::registerGlobals(CLuaH::luaState &L)
{

	return 0;
}

int GPPGame::loadSingleTexture(lua_State *L)
{
	CLuaFunctions::LuaParams p(L);

	if (p.getNumParams() == 2)
	{
		std::string path, name;
		p >> path;
		p >> name;

		p << GPPGame::GuitarPP().loadTexture(path, name, &CLuaH::Lua().getLuaStateScript(L)).getTextId();
	}
	else
	{
		p << 0;
	}
	return p.rtn();
}

int GPPGame::getGameplayRunningTime(lua_State *L)
{
	CLuaFunctions::LuaParams p(L);

	p << GPPGame::GuitarPP().gameplayRunningTime;

	return p.rtn();
}

int GPPGame::getDeltaTime(lua_State *L)
{
	CLuaFunctions::LuaParams p(L);

	p << CEngine::engine().getDeltaTime();

	return p.rtn();
}

int GPPGame::getGamePlayPlusState(lua_State *L)
{
	CLuaFunctions::LuaParams p(L);

	p << GPPGame::GuitarPP().gamePlayPlusEnabled;

	return p.rtn();
}

GPPGame::GPPGame() : glanguage("PT-BR"), gppTextureKeepBuffer(false), devMenus(newNamedMenu("devMenus")), uiRenameMenu("uiRenameMenu")
{
	songVolume = 0.8f;
	drawGamePlayBackground = true;
	showTextsTest = true;

	CLuaFunctions::LuaF().registerLuaFuncsAPI(registerFunctions);
	CLuaFunctions::LuaF().registerLuaFuncsAPI(registerGlobals);

	gameplayRunningTime = 0.0;

	mainSave.loadn("data/saves/mains");
	devMenus.gameMenu = true;
	uiRenameMenu.gameMenu = true;
	gamePlayPlusEnabled = false;

	hyperSpeed = 1.0;

	botEnabled = false;
	usarPalheta = true;
	CLuaFunctions::GameVariables::gv().pushVar("botEnabled", botEnabled);
	CLuaFunctions::GameVariables::gv().pushVar("usarPalheta", usarPalheta);
	CLuaFunctions::GameVariables::gv().pushVar("multiplayerClientIP", ip);
	CLuaFunctions::GameVariables::gv().pushVar("multiplayerPort", port);
	CLuaFunctions::GameVariables::gv().pushVar("defaultGuitar", defaultGuitar);

	CEngine::engine().loadSoundStream("data/sounds/erro-verde.wav", errorsSound[0]);
	CEngine::engine().loadSoundStream("data/sounds/erro-vermelho.wav", errorsSound[1]);
	CEngine::engine().loadSoundStream("data/sounds/erro-amarelo.wav", errorsSound[2]);
	CEngine::engine().loadSoundStream("data/sounds/erro-azul.wav", errorsSound[3]);
	CEngine::engine().loadSoundStream("data/sounds/erro-laranja.wav", errorsSound[4]);

	CEngine::engine().loadSoundStream("data/sounds/fretboard-inicio.wav", startSound);
	CEngine::engine().loadSoundStream("data/sounds/fretboard-fimdamusica.wav", endSound);

	CEngine::engine().renderFrameCallback = callbackRenderFrame;

	windowCFGs = getWindowDefaults();

	mainSave.addVariableAttData("windowCFGs", windowCFGs, true);
	mainSave.addVariableAttData("glanguage", glanguage, true);

	windowCFGs.name = std::string("");

	srand((unsigned int)time(0));

	devMode = false;

	mainMenu = nullptr;

	HUDText = 0;
	fretboardText = 0;

	static int uiRenameMenuText = 0;

	uiRenameMenu.qbgd.alphaBottom = 1.0;
	uiRenameMenu.qbgd.alphaTop = 1.0;

	{
		uiRenameMenu.qbgd.x1 = -0.1;
		uiRenameMenu.qbgd.x2 = -0.1 + 1.5;
		uiRenameMenu.qbgd.x3 = -0.1 + 1.5;
		uiRenameMenu.qbgd.x4 = -0.1;

		uiRenameMenu.qbgd.y1 = -0.6 + 0.75;
		uiRenameMenu.qbgd.y2 = -0.6 + 0.75;
		uiRenameMenu.qbgd.y3 = -0.6;
		uiRenameMenu.qbgd.y4 = -0.6;

		uiRenameMenu.qbgd.TextureX1 = 0.0;
		uiRenameMenu.qbgd.TextureX2 = 1.0;

		uiRenameMenu.qbgd.TextureY1 = 1.0;
		uiRenameMenu.qbgd.TextureY2 = 0.0;

		uiRenameMenu.qbgd.Text = 0;
	}

	{
		CMenu::menuOpt opt;

		opt.text = "";
		opt.y = -0.6 + 0.75;
		opt.x = -0.1;
		opt.deslizantBarSize = 1.5;
		opt.size = 0.1;
		opt.group = 2;
		opt.status = 0;
		opt.type = CMenu::menusOPT::drag_bar;

		CEngine::colorRGBToArray(0x3F51B5, opt.color);

		uiRenameMenu.addOpt(opt);
	}

	{
		CMenu::menuOpt opt;

		opt.text = "aaaa";
		opt.y = 0.05;
		opt.x = 0.0;
		opt.size = 0.075;
		opt.group = 1;
		opt.status = 0;
		opt.type = CMenu::menusOPT::text_input;

		uiRenameMenuText = uiRenameMenu.addOpt(opt);
	}


	{
		CMenu::menuOpt opt;

		opt.text = "Close";
		opt.y = -0.5;
		opt.x = 0.0;
		opt.size = 0.075;
		opt.group = 1;
		opt.status = 0;
		opt.type = CMenu::menusOPT::textbtn;
		opt.goback = true;
		opt.color[0] = 1.0;
		opt.color[1] = 0.0;
		opt.color[2] = 0.0;

		uiRenameMenu.addOpt(opt);
	}

	CEngine::engine().errorCallbackFun = logError;

	{
		CMenu::menuOpt opt;

		opt.text = "Renomear";
		//opt.langEntryKey = "devMenuRename";
		opt.y = 0.8;
		opt.x = -1.0;
		opt.size = 0.075;
		opt.group = 1;
		opt.status = 0;
		opt.type = CMenu::menusOPT::textbtn;

		static auto devMenuRename = [](const std::string &name)
		{
			auto &gpp = GPPGame::GuitarPP();

			if (gpp.devMenus.devEditMenu && gpp.devMenus.getUIListSize() == 0)
			{
				int op = gpp.devMenus.devEditMenu->getDevSelectedMenuOpt();

				if (op != -1)
				{
					gpp.devMenus.devEditingOpt = op;

					int r = gpp.devMenus.pushUserInterface(gpp.uiRenameMenu);

					CMenu *instM = gpp.devMenus.getUiAt(r).m.get();

					if (instM)
					{
						instM->options[uiRenameMenuText].preText = gpp.devMenus.devEditMenu->options[op].text;
						instM->options[uiRenameMenuText].externalPreTextRef = &(gpp.devMenus.devEditMenu->options[op].text);
					}

					gpp.devMenus.devMenuNOUpdateOthers = true;
				}
			}
		};

		opt.menusXRef.push_back(addGameCallbacks("devMenuRename", devMenuRename));

		devMenus.addOpt(opt);
	}

	// Load lua scripts from "data" folder
	CLuaH::Lua().loadFiles("data");
}


GPPGame::~GPPGame() noexcept
{
	try
	{
		mainSave.saves();
	}
	catch (...)
	{

	}
}

