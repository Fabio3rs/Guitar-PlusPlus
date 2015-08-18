#include "GPPGame.h"
#include "CLuaH.hpp"
#include "CEngine.h"
#include <cstdio>
#include <cstdlib>
#include "CLog.h"

/*
Provide default settings for the window
*/
GPPGame::gameWindow GPPGame::getWindowDefaults(bool safeMode){
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

CMenu &GPPGame::getMenuByName(const std::string &name){
	return gameMenus[name];
}

CMenu &GPPGame::newMenu(){
	CMenu m;
	gameMenus[m.getName()] = m;
	return gameMenus[m.getName()];
}

CMenu &GPPGame::newNamedMenu(const std::string &name){
	CMenu m(name);
	gameMenus[m.getName()] = m;
	return gameMenus[m.getName()];
}

void GPPGame::settWindowConfigs(const gameWindow &w){
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

GPPGame &GPPGame::GuitarPP(){
	static GPPGame game;
	return game;
}

double GPPGame::getWindowProportion(){
	return CEngine::engine().windowWidth / CEngine::engine().windowHeight;
}

const GPPGame::gppTexture &GPPGame::loadTexture(const std::string &path, const std::string &texture, CLuaH::luaScript *luaScript){
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
	if (themeName.size() == 0){
		CLog::log() << "GPPGame::CTheme::load(): Load theme fail - no name setted";
		return false;
	}

	main = CLuaH::Lua().newScript(themePath + themeName, "Theme.lua");

	if (main.luaState == nullptr){
		CLog::log() << "GPPGame::CTheme::load(): Load theme fail - Theme.lua wasn't loaded";
		return false;
	}

	CLuaH::Lua().runScript(main);


}

const GPPGame::CTheme &GPPGame::loadThemes(const std::string &path, const std::string &theme/*, CLuaH::luaScript *luaScript*/)
{
	/*auto &themeInst = gThemes[(path + "/" + theme)];

	if (!themeInst.isloaded())
	{
		gThemes[(path + "/" + theme)] = CTheme(path, theme);
	}*/
	
	return gThemes[(path + "/" + theme)];
}

void GPPGame::CTheme::apply()
{

}

GPPGame::CTheme::CTheme(const std::string &path, const std::string &theme)
{
	themePath = path;
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
int GPPGame::createWindow(){
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

	if (getWindowConfig().VSyncMode >= 0 && getWindowConfig().VSyncMode <= 2){
		setVSyncMode(getWindowConfig().VSyncMode);
	}

	CLuaH::Lua().runEvent("posCreateWindow");
	return CEngine::engine().windowOpened(); // Is the window really open?
}

void GPPGame::setVSyncMode(int mode)
{
	CEngine::engine().setVSyncMode(mode);
}

void GPPGame::setMainMenu(CMenu &m){
	mainMenu = &m;
}

CMenu *GPPGame::getMainMenu(){
	return mainMenu;
}

GPPGame::GPPGame(){
	// Load lua scripts from "data" folder
	CLuaH::Lua().loadFiles("data");

	windowCFGs = getWindowDefaults();

	srand((unsigned int)time(0));

	mainMenu = nullptr;
}
