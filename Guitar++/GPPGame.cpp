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
	module.players.back().loadSongOnlyChart("TTFAF");

	l.processing = false;
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

	module.players.push_back(CPlayer("xi"));

	loadThreadData l;

	l.processing = true;

	std::thread load(loadThread, std::ref(module), std::ref(l));

	while (CEngine::engine().windowOpened() && l.processing) {
		GPPGame::GuitarPP().clearScreen();

		CFonts::fonts().drawTextInScreen("loading", -0.4, 0.0, 0.1);

		GPPGame::GuitarPP().renderFrame();
	}


	double startTime = module.players.back().startTime = CEngine::engine().getTime() + 3.0;
	double openMenuTime = 0.0;
	module.players.back().musicRunningTime = -3.0;

	bool enterInMenu = false, esc = false;

	while (CEngine::engine().windowOpened()) {
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
			openMenuTime = CEngine::engine().getTime();

			game.openMenus(game.getMainMenu());

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




			module.render();

			double time = CEngine::engine().getTime();

			if ((startTime - time) > 0.0)
			{
				CFonts::fonts().drawTextInScreen(std::to_string((int)(startTime - time)), -0.3, 0.0, 0.3);
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

GPPGame::GPPGame()
{
	// Load lua scripts from "data" folder
	CLuaH::Lua().loadFiles("data");

	windowCFGs = getWindowDefaults();

	srand((unsigned int)time(0));

	mainMenu = nullptr;
}
