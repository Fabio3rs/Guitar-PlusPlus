#include "GPPGame.h"
#include "CLuaH.hpp"
#include "CEngine.h"
#include <cstdio>
#include <cstdlib>
#include "CLog.h"
#include <filesystem>
#include <Windows.h>
#include "CControls.h"
#include <functional>
#include "CFonts.h"
#include "objloader.hpp"

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
	std::string song = GuitarPP().songToLoad;
	//module.players[0].loadSongOnlyChart(song);
	//module.players[1].loadSong(song);
	module.players.back().loadSong(song);

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

void GPPGame::startModule(const std::string &name)
{
	auto &game = GPPGame::GuitarPP();
	auto realname = game.getCallBackRealName(name);
	auto &module = game.gameModules[realname];

	if (game.getRunningModule().size() > 0)
	{
		throw gameException("A module is already running: " + name);
	}

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

		CFonts::fonts().drawTextInScreen("loading", -0.4, 0.0, 0.1);

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
	GPPGame::GuitarPP().HOPOSText = GPPGame::GuitarPP().loadTexture("data/sprites", "HOPOS.tga").getTextId();

	double startTime = module.players.back().startTime = CEngine::engine().getTime() + 3.0;
	double openMenuTime = 0.0;
	module.players.back().musicRunningTime = -3.0;

	bool enterInMenu = false, esc = false;
	int musicstartedg = 0;

	bool songTimeFixed = false;

	std::cout << "Plus in chart: " << module.players.back().Notes.gPlus.size() << std::endl;

	if (module.players.back().Notes.gPlus.size())
	{
		std::cout << "Plus 0 time: " << module.players.back().Notes.gPlus[0].time << std::endl;
		std::cout << "Plus 0 duration: " << module.players.back().Notes.gPlus[0].lTime << std::endl;
	}

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

			double time = CEngine::engine().getTime();

			if (musicstartedg == 0) musicstartedg = 1;

			if ((startTime - time) > 0.0)
			{
				CFonts::fonts().drawTextInScreen(std::to_string((int)(startTime - time)), -0.3, 0.0, 0.3);
				musicstartedg = 0;
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
		return std::string((s = fname.find_last_of('.') != fname.npos) ? &fname.c_str()[++s] : "");
	};

	auto file_exists = [](const std::string &fileName)
	{
		return std::fstream(fileName).is_open();
	};

	// TODO: change to STD FileSystem

	HANDLE hFind;
	WIN32_FIND_DATA data;

	hFind = FindFirstFile((path + "/*").c_str(), &data);

	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			const std::string name = data.cFileName;

			if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && name != ".." && name != "."
				&& file_exists((path + std::string("/") + name + "/Theme.lua")))
			{
				loadThemes(name);
			}
		} while (FindNextFile(hFind, &data));
		FindClose(hFind);
	}

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

void GPPGame::openMenus(CMenu *startMenu)
{
	auto &engine = CEngine::engine();
	auto &lua = CLuaH::Lua();

	std::deque < CMenu* > menusStack;

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

	while (menusStack.size() != 0 && engine.windowOpened())
	{
		clearScreen();

		engine.activate3DRender(false);

		auto &menu = *menusStack.back();
		currentMenu = &menu;

		menu.update();

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

		menu.render();

		for (auto &opt : menu.options)
		{
			if ((opt.status & 3) == 3)
			{
				if (opt.goback)
				{
					// Clean a temporary menu
					if (menu.temp)
					{
						gameMenus.erase(menu.getName());
					}

					menusStack.pop_back();

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

		//engine.bindTextOnSlot(0, 0);

		//engine.attribVBOBuff(0, 3, vertexbuffer);
		//engine.attribVBOBuff(1, 2, uvbuffer);

		//engine.RenderCustomVericesFloat(&vertices[0], &uvs[0], vertices.size());

		//engine.disableBuf(0);
		//engine.disableBuf(1);

		const char prog[] = { "Programado por Fabio Rossini Sluzala" };
		const char brmods[] = { "http://brmodstudio.forumeiros.com/" };
		const char oneByte[] = { "http://2nibble.forumeiros.com/" };
		
		CFonts::fonts().drawTextInScreen(prog, CFonts::fonts().getCenterPos(strlen(prog), 0.08, 0.0), -0.8, 0.08);

		CEngine::engine().setColor(0.5, 0.5, 1.0, 1.0);

		CFonts::fonts().drawTextInScreen(brmods, CFonts::fonts().getCenterPos(strlen(brmods), 0.05, 0.0), -0.87, 0.05);
		CFonts::fonts().drawTextInScreen(oneByte, CFonts::fonts().getCenterPos(strlen(oneByte), 0.05, 0.0), -0.93, 0.05);

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

	return CEngine::engine().windowOpened(); // Is the window really open?
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

GPPGame::GPPGame() : noteOBJ("data/models/GPP_Note.obj"), triggerBASEOBJ("data/models/TriggerBase.obj"), triggerOBJ("data/models/Trigger.obj")
{
	// Load lua scripts from "data" folder
	CLuaH::Lua().loadFiles("data");

	CEngine::engine().loadSoundStream("data/sounds/erro-verde.wav", errorsSound[0]);
	CEngine::engine().loadSoundStream("data/sounds/erro-vermelho.wav", errorsSound[1]);
	CEngine::engine().loadSoundStream("data/sounds/erro-amarelo.wav", errorsSound[2]);
	CEngine::engine().loadSoundStream("data/sounds/erro-azul.wav", errorsSound[3]);
	CEngine::engine().loadSoundStream("data/sounds/erro-laranja.wav", errorsSound[4]);

	windowCFGs = getWindowDefaults();

	srand((unsigned int)time(0));

	devMode = false;

	mainMenu = nullptr;

	HUDText = 0;
	fretboardText = 0;

	CEngine::engine().errorCallbackFun = logError;
}
