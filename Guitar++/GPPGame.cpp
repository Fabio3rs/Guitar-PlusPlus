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
#include "objloader.hpp"
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

					CEngine::engine().setSoundVolume(gp.players.back().songAudioID, 0.8);

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
			if (!botK && CEngine::engine().getKey('B'))
			{
				botK = true;
				module.players.back().enableBot ^= 1;
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


	std::atomic<int> state = 0;

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


	module.players.push_back(CPlayer("xi"));

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


	module.players.back().enableBot = GPPGame::GuitarPP().botEnabled;
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
			opt.x = CFonts::fonts().getCenterPos(opt.text.size(), opt.size, 0.0);
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
			opt.x = CFonts::fonts().getCenterPos(opt.text.size(), opt.size, 0.0);
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
	gameMenus[m.getName()] = m;
	gameMenus[m.getName()].gameMenu = true;
	return gameMenus[m.getName()];
}

CMenu &GPPGame::newNamedMenu(const std::string &name)
{
	CMenu m(name);
	gameMenus[m.getName()] = m;
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

	gTextures[(path + "/" + texture)] = gppTexture(path, texture);
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

GPPGame::func_t GPPGame::getCallback(const std::string &str)
{
	return gameCallbacks[str];
}

std::string GPPGame::getRunningModule()
{
	return runningModule;
}

void GPPGame::openMenus(CMenu *startMenu, std::function<int(void)> preFun, std::function<int(void)> midFun, std::function<int(void)> posFun, bool dev)
{
	auto &engine = CEngine::engine();
	auto &lua = CLuaH::Lua();
	double waitForTime = 0.0;
	
	std::deque < CMenu* > menusStack;
	//static std::deque < CMenu* > devMenusStack;

	menusStack.push_back(startMenu);

	currentMenu = startMenu;

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

	while (menusStack.size() != 0 && engine.windowOpened())
	{
		clearScreen();
		updateRender = (engine.getTime() - waitForTime) >= 0.0;

		engine.activate3DRender(false);

		auto &menu = *menusStack.back();
		currentMenu = &menu;

		if (dev)
		{
			devMenus.devEditMenu = &menu;
		}

		if (preFun)
			preFun();

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

		if (RenderData.Text = texture.getTextId())
		{
			double prop = (double)texture.getImgWidth() / (double)texture.getImgHeight();

			RenderData.x1 = -prop;
			RenderData.x2 = prop;
			RenderData.x3 = prop;
			RenderData.x4 = -prop;

			engine.Render2DQuad(RenderData);
		}

		if (midFun)
			midFun();

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
					}
					else{
						lua.runEvent("menusNext");
					}
					break;
				}
			}
		}

		eraseGameMenusAutoCreateds();


		if (posFun)
			posFun();

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
		CShader::inst();
		CShader::inst().addEvent("test");
		int l0 = CShader::inst().newShader("vert.vert", 0, "test");
		int l = CShader::inst().newShader("frag.frag", 1, "test");
		CShader::inst().addShaderToEvent("test", l0);
		CShader::inst().addShaderToEvent("test", l);
		CShader::inst().linkAllShaders();
	}
	catch (std::exception &e)
	{
		CLog::log() << e.what();
	}


	CLuaH::Lua().runEvent("posCreateWindow");

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

	for (auto &sb : sbaseTexture3D)
	{
		sb = loadTexture("data/sprites", "base" + std::to_string(itext++) + ".tga").getTextId();
	}

	itext = 0;

	for (auto &st : striggerTexture3D)
	{
		st = loadTexture("data/sprites", "trigger" + std::to_string(itext++) + ".tga").getTextId();

		std::cout << st << std::endl;
	}



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

GPPGame::GPPGame() : noteOBJ("data/models/GPP_Note.obj"), triggerBASEOBJ("data/models/TriggerBase.obj"),
						triggerOBJ("data/models/Trigger.obj"), pylmbarOBJ("data/models/pylmbar.obj"),
						devMenus(newNamedMenu("devMenus")), uiRenameMenu("uiRenameMenu")
{
	mainSave.loadn("data/saves/mains");
	devMenus.gameMenu = true;
	uiRenameMenu.gameMenu = true;

	glanguage = "PT-BR";

	// Load lua scripts from "data" folder
	CLuaH::Lua().loadFiles("data");

	hyperSpeed = 1.0;

	botEnabled = false;
	CLuaFunctions::GameVariables::gv().pushVar("botEnabled", botEnabled);
	CLuaFunctions::GameVariables::gv().pushVar("multiplayerClientIP", ip);
	CLuaFunctions::GameVariables::gv().pushVar("multiplayerPort", port);

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

		uiRenameMenu.addOpt(opt);
	}

	{
		CMenu::menuOpt opt;

		opt.text = "aaaa";
		opt.y = 0.075;
		opt.x = 0.0;
		opt.size = 0.05;
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

					CMenu *instM = gpp.devMenus.getUiAt(r).m;

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
}


GPPGame::~GPPGame()
{
	mainSave.saves();
}

