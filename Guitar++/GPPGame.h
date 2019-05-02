#pragma once
#ifndef _GUITAR_PP_CGPPGAME_h_
#define _GUITAR_PP_CGPPGAME_h_

#include <string>
#include <map>
#include <memory>
#include <deque>
#include <vector>
#include <exception>
#include <atomic>
#include <thread>
#include <mutex>
#include "CShader.h"
#include "CMenu.h"
#include "CEngine.h"
#include "GPPOBJ.h"
#include "CSaveSystem.h"
#include "CGamePlay.h"
#include "CLuaFunctions.hpp"
#include "CPlayer.h"

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

	bool devMode;

	// Script events
	static int firstStartFrameSE, preCreateWindowSE, posCreateWindowSE, preLoadSpritesSE, posClearScreenSE,
		preRenderFrameSE, posRenderFrameSE, menusGoBackSE, menusCMMenuSE, menusNextSE, menusGameCbNextSE, catchedExceptionSE,
		joystickStateCbSE;

public:
	class MessageTypes
	{
		std::string fontName;
		double minTime;

		MessageTypes()
		{
			minTime = 0.0;
		}
	};

	typedef std::function<void(const std::string &name)> func_t;

	CMenu devMenus, uiRenameMenu, uiCreateProfile;

	int uiCreateUITextID, uiCreateUIPathID;

	std::string glanguage;

	CSaveSystem::CSave mainSave;

	bool gppTextureKeepBuffer, drawGamePlayBackground, showTextsTest;
	float songVolume;

	// Texture instance manager
	class gppTexture{
		friend GPPGame;
		unsigned int text;
		std::string textPath;
		std::string textName;

		CEngine::GLFWimage imgData;

		std::map < CLuaH::luaScript*, bool > associatedToScript;

		// DO NOT DUPLICATE THE TEXTURE INSTANCE!!!!!!!
		gppTexture(const gppTexture&) = delete;

	public:
		unsigned int getTextId() const noexcept
		{
			return text;
		}

		std::string getTexturePath() const noexcept
		{
			return textPath;
		}

		std::string getTextureName() const noexcept
		{
			return textName;
		}

		std::string getGTextureName() const
		{
			return (getTexturePath() + "/" + getTextureName());
		}

		int getImgWidth() const noexcept
		{
			return imgData.Width;
		}

		int getImgHeight() const noexcept
		{
			return imgData.Height;
		}

		inline CEngine::GLFWimage &getImageData() noexcept
		{
			return imgData;
		}

		gppTexture(const std::string &path, const std::string &texture)
		{
			imgData.keepData = GPPGame::GuitarPP().gppTextureKeepBuffer;

			text = CEngine::engine().loadTexture((path + std::string("/") + texture).c_str(), &imgData);
			textPath = path;
			textName = texture;
		}

		gppTexture &operator = (gppTexture &&m) noexcept
		{
			if (this == std::addressof(m)) return *this;

			text = std::move(m.text);
			textPath = std::move(m.textPath);
			textName = std::move(m.textName);
			imgData = std::move(m.imgData);
			associatedToScript = std::move(m.associatedToScript);

			if (m.text)
				m.text = 0;

			return *this;
		};

		gppTexture(gppTexture&&) = default;

		gppTexture() noexcept
		{
			text = 0;
		}

		~gppTexture() noexcept
		{
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

		CTheme(const CTheme&) = delete;

		static int applyThemeSE;

	public:
		void apply();

		inline bool isloaded() const{ return loaded; }

		CTheme(const std::string &theme);
		CTheme(CTheme&&) = default;
		CTheme();
	};

	CMenu &newMenu();
	CMenu &newNamedMenu(const std::string &name);

	void loadBasicSprites();
	void loadBasicSounds();

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

	//std::map <std::string, fretsPosition> frets;

	void setDevMode(bool mode);

	std::map<std::string, bool>				 cmdparams;
	void parseParameters(int argc, char *argv[]);
	
	const gppTexture &loadTexture(const std::string &path, const std::string &texture, CLuaH::luaScript *luaScript = nullptr);
	int loadTextureGetId(const std::string &path, const std::string &texture, CLuaH::luaScript *luaScript = nullptr);
	unsigned int getTextureId(const std::string &name) const noexcept;
	const CTheme &loadThemes(const std::string &theme, CLuaH::luaScript *luaScript = nullptr);
	const std::string addGameCallbacks(const std::string &n, func_t function);

	std::string getCallBackRealName(const std::string &str);
	std::string selectSong();
	void addSongListToMenu(CMenu &selectSongMenu, std::map<int, std::string> &menuMusics);

	static void teste(const std::string &name);
	static void startModule(const std::string &name);
	static void startMarathonModule(const std::string &name);
	static void serverModule(const std::string &name);
	static void continueCampaing(const std::string &name);
	static void campaingPlayModule(const std::string &name);
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

	std::vector <CMenu*> openMenus(CMenu *startMenu, std::function<int(void)> preFun = nullptr, std::function<int(void)> midFun = nullptr, std::function<int(void)> posFun = nullptr, bool dev = false, std::vector < CMenu* > stackTest = std::vector < CMenu* >());

	inline CMenu *getActualMenu()
	{
		return currentMenu;
	}

	static void helpMenu(const std::string &name);
	
	GPPOBJ noteOBJ, triggerBASEOBJ, triggerOBJ, pylmbarOBJ, openNoteOBJ;

	unsigned int strumsTexture3D[6];
	unsigned int hopoTexture3D[6];
	unsigned int tapTexture3D[6];

	unsigned int sbaseTexture3D[6];
	unsigned int striggerTexture3D[6];
	unsigned int openNoteTexture3D, openNoteHOPOTexture3D, openNotePTexture3D, openNoteHOPOPTexture3D;

	unsigned int strumKeys[5], fretOneKey, fretTwoKey;

	unsigned int HUDText, fretboardText, lineText, HOPOSText, pylmBarText;

	std::string songToLoad;

	int errorsSound[5], startSound, endSound;

	double hyperSpeed;

	bool botEnabled, usarPalheta;

	std::string defaultGuitar;

	static std::string ip, port;

	static std::mutex playersMutex;

	static void charterModule(const std::string &name);
	static void testClient(const std::string &name);

	double gameplayRunningTime;
	bool gamePlayPlusEnabled;

	std::deque<std::string> marathonSongsList;

	static std::deque<std::string> getDirectory(const char *dir, bool getFiles, bool getDirectories);

	void initialLoad();
	void initialLoad2();


	void selectPlayerMenu();

protected:
	static int registerFunctions(CLuaH::luaState &Lstate);
	static int registerGlobals(CLuaH::luaState &L);

	static int loadSingleTexture(lua_State *L);
	static int getGameplayRunningTime(lua_State *L);
	static int getDeltaTime(lua_State *L);
	static int getGamePlayPlusState(lua_State *L);

private:
	std::shared_ptr<CPlayer> mainPlayer;

	GPPOBJ testobj;

	static void callbackRenderFrame();
	static void callbackKeys(int key, int scancode, int action, int mods);
	static void callbackJoystick(int jid, int eventId);

	struct loadThreadData
	{
		std::atomic<bool> processing;
		std::atomic<bool> continueThread;
		std::atomic<bool> loadSong;
		std::atomic<bool> sendToModulePlayers;
		std::atomic<bool> listEnd;
		std::atomic<size_t> songID;

		std::deque<std::string> songsList;

		inline ~loadThreadData()
		{
			continueThread = false;
		}
	};

	static void loadThread(CGamePlay &module, loadThreadData &l);
	static void loadMarathonThread(CGamePlay &module, loadThreadData &l);

	void eraseGameMenusAutoCreateds();

	GPPGame(GPPGame&) = delete;

	bool			windowChangedParams; // update in case of realtime changes - TODO

	gameWindow		windowCFGs; // Actual config

	GPPGame();
	~GPPGame() noexcept;

public:
	const gameWindow &getWindowConfig() const{ return windowCFGs; };
	int createWindow();

	static void logError(int code, const std::string &e);
};

#endif
