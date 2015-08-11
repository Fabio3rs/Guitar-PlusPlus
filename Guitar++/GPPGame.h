#pragma once
#ifndef _GUITAR_PP_CGPPGAME_h_
#define _GUITAR_PP_CGPPGAME_h_

#include <string>
#include <unordered_map>
#include <memory>
#include <deque>
#include "CLuaFunctions.hpp"
#include "CMenu.h"

class GPPGame{
	std::unordered_map <std::string, CMenu> gameMenus;

	CMenu *mainMenu;

public:
	struct gameWindow{
		int h, w, AA, colorBits;
		bool fullscreen;
		std::string name; // "Guitar ++ - name"/"Guitar++"
	};

	CMenu &newMenu();
	CMenu &newNamedMenu(const std::string &name);

	void loadBasicSprites();

	void setMainMenu(CMenu &m);
	CMenu *getMainMenu();

	CMenu &getMenuByName(const std::string &name);

	// loaded game sprites
	std::unordered_map <std::string, int> SPR;

	// Window settings
	gameWindow getWindowDefaults(bool safeMode = false);
	void settWindowConfigs(const gameWindow &w);

	// Render basics
	void clearScreen();
	void renderFrame();

	double getWindowProportion();

	// Initer
	static GPPGame &GuitarPP();

	GPPGame(GPPGame&) = delete;
private:
	bool			windowChangedParams; // update in case of realtime changes - TODO

	gameWindow		windowCFGs; // Actual config

	GPPGame();

public:
	const gameWindow &getWindowConfig() const{ return windowCFGs; };
	int createWindow();
};

#endif
