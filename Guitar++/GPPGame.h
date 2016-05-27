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
#include "CGamePlay.h"
#include <exception>
#include <atomic>
#include <thread>
#include "GPPOBJ.h"
#include "CSaveSystem.h"

class gameException : public std::exception{
	std::string str;

public:
	const char *what() const
	{
		return str.c_str();
	}

	inline gameException(const std::string &s) : std::exception(s.c_str()), str(s)
	{

	}

	inline gameException()
	{

	}
};

class GPPGame{
	std::unordered_map <std::string, CMenu> gameMenus;

	CMenu *mainMenu;

	CMenu *currentMenu;

	std::string runningModule;

	bool devMode;


public:
	std::string glanguage;

	CSaveSystem::CSave mainSave;

	typedef void(*func_t)(const std::string &name);

	// Texture instance manager
	class gppTexture{
		friend GPPGame;
		unsigned int text;
		std::string textPath;
		std::string textName;

		CEngine::GLFWimage imgData;

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

		std::string getGTextureName() const{
			return (getTexturePath() + "/" + getTextureName());
		}

		int getImgWidth() const{
			return imgData.Width;
		}

		int getImgHeight() const{
			return imgData.Height;
		}

		gppTexture(const std::string &path, const std::string &texture){
			text = CEngine::engine().loadTexture((path + std::string("/") + texture).c_str(), &imgData);
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
		int h, w, AA, colorBits, VSyncMode;
		bool fullscreen;
		std::string name; // "Guitar ++ - name"/"Guitar++"
	};

	class CTheme{
		friend GPPGame;
		std::string themeName;

		bool loaded;

		bool load();
		const CLuaH::luaScript *main;

		CTheme(CTheme&) = delete;

	public:
		void apply();

		inline bool isloaded() const{ return loaded; }

		CTheme(const std::string &theme);
		CTheme();
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
	std::unordered_map <std::string, CTheme> gThemes;

	std::unordered_map <std::string, func_t> gameCallbacks;
	std::unordered_map <std::string, std::string> gameCallbacksWrapper;

	std::unordered_map <std::string, CGamePlay> gameModules;

	std::unordered_map <std::string, fretsPosition> frets;

	void setDevMode(bool mode);

	std::map<std::string, bool>				 cmdparams;
	void parseParameters(int argc, char *argv[]);

	std::string getRunningModule();
	
	const gppTexture &loadTexture(const std::string &path, const std::string &texture, CLuaH::luaScript *luaScript = nullptr);
	const CTheme &loadThemes(const std::string &theme, CLuaH::luaScript *luaScript = nullptr);
	const std::string addGameCallbacks(const std::string &n, func_t function);

	std::string getCallBackRealName(const std::string &str);

	static void teste(const std::string &name);
	static void startModule(const std::string &name);
	static void continueCampaing(const std::string &name);
	static void benchmark(const std::string &name);

	void loadAllThemes();

	// Window settings
	gameWindow getWindowDefaults(bool safeMode = false);
	void settWindowConfigs(const gameWindow &w);

	func_t getCallback(const std::string &str);

	// Render basics
	void clearScreen();
	void renderFrame();

	double getWindowProportion();

	// Initer
	static GPPGame &GuitarPP();

	void setVSyncMode(int mode);

	void openMenus(CMenu *startMenu, std::function<int(void)> preFun = nullptr, std::function<int(void)> midFun = nullptr, std::function<int(void)> posFun = nullptr);

	inline CMenu *getActualMenu()
	{
		return currentMenu;
	}

	static void helpMenu(const std::string &name);
	
	GPPOBJ noteOBJ, triggerBASEOBJ, triggerOBJ, pylmbarOBJ;

	unsigned int strumsTexture3D[6];
	unsigned int hopoTexture3D[6];

	unsigned int sbaseTexture3D[6];
	unsigned int striggerTexture3D[6];


	unsigned int strumKeys[5], fretOneKey, fretTwoKey;

	unsigned int HUDText, fretboardText, lineText, HOPOSText;

	std::string songToLoad;

	int errorsSound[5], startSound, endSound;

	double hyperSpeed;

	bool botEnabled;

private:
	struct loadThreadData{
		std::atomic<bool> processing;
	};

	static void loadThread(CGamePlay &module, loadThreadData &l);

	void eraseGameMenusAutoCreateds();

	GPPGame(GPPGame&) = delete;

	bool			windowChangedParams; // update in case of realtime changes - TODO

	gameWindow		windowCFGs; // Actual config

	GPPGame();
	~GPPGame();

	void setRunningModule(const std::string m);

public:
	const gameWindow &getWindowConfig() const{ return windowCFGs; };
	int createWindow();

	static void logError(int code, const std::string &e);
};

#endif
