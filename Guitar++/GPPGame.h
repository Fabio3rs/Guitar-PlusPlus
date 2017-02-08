#pragma once
#ifndef _GUITAR_PP_CGPPGAME_h_
#define _GUITAR_PP_CGPPGAME_h_

#include <string>
#include <map>
#include <memory>
#include <deque>
#include "CLuaFunctions.hpp"
#include "CMenu.h"
#include "CEngine.h"
#include "CGamePlay.h"
#include <exception>
#include <atomic>
#include <thread>
#include <mutex>
#include "GPPOBJ.h"
#include "CSaveSystem.h"
#include "CShader.h"

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
	std::map <std::string, CMenu> gameMenus;

	CMenu *mainMenu;

	CMenu *currentMenu;

	std::string runningModule;

	bool devMode;

public:
	CMenu devMenus, uiRenameMenu;

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

		std::map < CLuaH::luaScript*, bool > associatedToScript;

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
		int fullscreen;
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
	std::map <std::string, int> SPR;
	std::map <std::string, gppTexture> gTextures;
	std::map <std::string, CTheme> gThemes;

	std::map <std::string, func_t> gameCallbacks;
	std::map <std::string, std::string> gameCallbacksWrapper;

	std::map <std::string, CGamePlay> gameModules;

	std::map <std::string, fretsPosition> frets;

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
	static void startMarathonModule(const std::string &name);
	static void serverModule(const std::string &name);
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

	void openMenus(CMenu *startMenu, std::function<int(void)> preFun = nullptr, std::function<int(void)> midFun = nullptr, std::function<int(void)> posFun = nullptr, bool dev = false);

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

	unsigned int HUDText, fretboardText, lineText, HOPOSText, pylmBarText;

	std::string songToLoad;

	int errorsSound[5], startSound, endSound;

	double hyperSpeed;

	bool botEnabled;

	static void charterModule(const std::string &name);

	static std::string ip, port;

	static void testClient(const std::string &name);

	static std::mutex playersMutex;

	std::string defaultGuitar;

protected:
	static int registerFunctions(lua_State *L);
	static int registerGlobals(lua_State *L);

	static int loadSingleTexture(lua_State *L);

private:
	GPPOBJ testobj;

	static void callbackRenderFrame();

	struct loadThreadData
	{
		std::atomic<bool> processing;
		std::atomic<bool> continueThread;
		std::atomic<bool> loadSong;
		std::atomic<bool> sendToModulePlayers;
		std::atomic<bool> listEnd;
		std::atomic<int> songID;

		std::deque<std::string> songsList;

		inline ~loadThreadData()
		{
			continueThread = false;
		}
	};

	static void loadThread(CGamePlay &module, loadThreadData &l);
	static void loadMarathonThread(CGamePlay &module, loadThreadData &l);
	static std::deque<std::string> getDirectory(const char *dir, bool getFiles, bool getDirectories);

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
