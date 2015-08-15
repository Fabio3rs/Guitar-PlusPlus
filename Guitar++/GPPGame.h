#pragma once
#ifndef _GUITAR_PP_CGPPGAME_h_
#define _GUITAR_PP_CGPPGAME_h_

#include <string>
#include <unordered_map>
#include <memory>
#include <deque>
#include "CLuaFunctions.hpp"
#include "CMenu.h"
#include "CEngine.h"

class GPPGame{
	std::unordered_map <std::string, CMenu> gameMenus;

	CMenu *mainMenu;

public:
	// Texture instance manager
	class gppTexture{
		friend GPPGame;
		unsigned int text;
		std::string textPath;
		std::string textName;

		std::unordered_map < CLuaH::luaScript*, bool > associatedToScript;

		// DO NOT DUPLICATE THE TEXTURE INSTANCE!!!!!!!
		gppTexture(gppTexture&) = delete;

	public:
		unsigned int getTextId() const{
			return text;
		}

		std::string getTexturePath() const{
			return textPath;
		}

		std::string getTextureName() const{
			return textName;
		}

		gppTexture(const std::string &path, const std::string &texture){
			text = CEngine::engine().loadTexture((path + std::string("/") + texture).c_str());
			textPath = path;
			textName = texture;
		}

		gppTexture(){
			text = 0;
		}

		~gppTexture(){
			text = 0;
			// TODO CEngine::engine().unloadTexture(id);
		}
	};

	struct gameWindow{
		int h, w, AA, colorBits;
		bool fullscreen;
		std::string name; // "Guitar ++ - name"/"Guitar++"
	};

	class CTheme{
		std::string name;

		bool load();
		CLuaH::luaScript main;

	public:

		CTheme(const std::string &name);
	};

	CMenu &newMenu();
	CMenu &newNamedMenu(const std::string &name);

	void loadBasicSprites();

	void setMainMenu(CMenu &m);
	CMenu *getMainMenu();

	CMenu &getMenuByName(const std::string &name);

	// loaded game sprites - TODO: improve it
	std::unordered_map <std::string, int> SPR;
	std::unordered_map <std::string, gppTexture> gTextures;
	
	const gppTexture &loadTexture(const std::string &path, const std::string &texture, CLuaH::luaScript *luaScript = nullptr);

	// Window settings
	gameWindow getWindowDefaults(bool safeMode = false);
	void settWindowConfigs(const gameWindow &w);

	// Render basics
	void clearScreen();
	void renderFrame();

	double getWindowProportion();

	// Initer
	static GPPGame &GuitarPP();

private:
	GPPGame(GPPGame&) = delete;

	bool			windowChangedParams; // update in case of realtime changes - TODO

	gameWindow		windowCFGs; // Actual config

	GPPGame();

public:
	const gameWindow &getWindowConfig() const{ return windowCFGs; };
	int createWindow();
};

#endif
