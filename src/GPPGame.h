#pragma once
#ifndef _GUITAR_PP_CGPPGAME_h_
#define _GUITAR_PP_CGPPGAME_h_

#include "CEngine.h"
#include "CGamePlay.h"
#include "CGameSettings.h"
#include "CLuaFunctions.hpp"
#include "CMenu.h"
#include "CPlayer.h"
#include "CScene.h"
#include "CShader.h"
#include "CTextureManager.h"
#include "GPPOBJ.h"
#include "ui/CMenuSystem.h"
#include <atomic>
#include <cctype>
#include <deque>
#include <exception>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class gameException : public std::exception {
    std::string str;

  public:
    const char *what() const noexcept { return str.c_str(); }

    inline gameException(const std::string &s) noexcept
        : std::exception(), str(s) {}

    inline gameException() noexcept {}
};

class GPPGame {

    std::vector<SongResult> songResults;

    bool devMode;

    // Script events
    static int firstStartFrameSE, preCreateWindowSE, posCreateWindowSE,
        preLoadSpritesSE, posClearScreenSE, preRenderFrameSE, posRenderFrameSE,
        catchedExceptionSE, joystickStateCbSE;

  public:
    std::atomic<std::thread::id> mainthread;

    class MessageTypes {
        std::string fontName;
        double minTime;

        MessageTypes() { minTime = 0.0; }
    };

    typedef std::function<void(const std::string &name)> func_t;

    CMenuSystem menus;
    void resetMainPlayer();

    std::string glanguage;

    bool drawGamePlayBackground, showTextsTest;
    float songVolume;

    // Compatibility view. The manager owns resource identity and GPU lifetime.
    class gppTexture {
        friend GPPGame;
        CTextureManager *manager = nullptr;
        CTextureManager::Handle resource;
        std::string textPath, textName;
        bool asynchronous = false;
        std::map<CLuaH::luaScript *, bool> associatedToScript;

      public:
        gppTexture() = default;
        gppTexture(CTextureManager &owner, CTextureManager::Handle handle);
        gppTexture(const std::string &path, const std::string &texture,
                   bool async = false);
        gppTexture(const gppTexture &) = delete;
        gppTexture &operator=(const gppTexture &) = delete;
        gppTexture(gppTexture &&other) noexcept;
        gppTexture &operator=(gppTexture &&other) noexcept;
        ~gppTexture() = default;

        bool isAsyncRunning() const;
        unsigned int getTextId() const;
        unsigned int getTextIdUpdateAsync();
        std::string getTexturePath() const { return textPath; }
        std::string getTextureName() const { return textName; }
        std::string getGTextureName() const {
            return textPath + "/" + textName;
        }
        int getImgWidth() const;
        int getImgHeight() const;
        std::shared_ptr<const CTextureImage> getImageData() const;
        void load();
        void tloadAsync(const std::string &path, const std::string &texture);
    };

    struct loadTextureBatch {
        std::string path;
        std::string texture;
        CLuaH::luaScript *luaScript;

        std::function<void(loadTextureBatch *)> targetFun;
        void *userptr;
        std::string username;

        double addedTime;
        gppTexture *text;

        loadTextureBatch() {
            luaScript = nullptr;
            text = nullptr;
            userptr = nullptr;
            addedTime = 0.0;
        }

        loadTextureBatch(std::string pa, std::string tx, void *up,
                         std::function<void(loadTextureBatch *)> tf,
                         const std::string &usrnm = "",
                         CLuaH::luaScript *ls = nullptr) {
            path = pa;
            texture = tx;
            userptr = up;
            targetFun = tf;
            username = usrnm;

            luaScript = ls;
            text = nullptr;
            addedTime = 0.0;
        }
    };

  private:
    std::unique_ptr<CTextureManager> textureManager;
    mutable std::mutex textureViewsMutex;
    // Stable legacy views and script associations, not a second resource cache.
    std::map<std::string, gppTexture> textureViews;
    gppTexture unavailableTexture;
    gppTexture &textureView(const CTextureManager::Handle &resource,
                            CLuaH::luaScript *script = nullptr);

  public:
    // Lifecycle boundaries only: no callers may overlap reinitialization.
    void initializeTextures();
    void shutdownTextures();
    CTextureResource::Snapshot getTextureInfo(const std::string &key) const;

    bool loadTextureSingleAsync(const loadTextureBatch &tData);
    void forceTexturesToLoad();

    void textureStreammingProcess();
    void streammingProcess();
    int getNumTexturesToLoad();

    using gameWindow = CGameSettings::Window;

    class CTheme {
        friend GPPGame;
        std::string themeName;

        bool loaded;

        bool load();
        const CLuaH::luaScript *main{};

        CTheme(const CTheme &) = delete;

        static int applyThemeSE;

      public:
        void apply();

        inline bool isloaded() const { return loaded; }

        CTheme(const std::string &theme);
        CTheme(CTheme &&) = default;
        CTheme();
    };

    void loadBasicSprites();
    void loadBasicSounds();

    // loaded game sprites - TODO: improve it
    std::map<std::string, int> SPR;
    std::map<std::string, CTheme> gThemes;

    std::map<std::string, func_t> gameCallbacks;
    std::map<std::string, std::string> gameCallbacksWrapper;

    std::map<std::string, CGamePlay> gameModules;

    // std::map <std::string, fretsPosition> frets;

    void setDevMode(bool mode);

    std::map<std::string, bool> cmdparams;
    void parseParameters(int argc, char *argv[]);

    struct loadModelBatch {
        GPPOBJ *obj = nullptr;
        std::string path, model;
        CModelManager::Handle handle;
        loadModelBatch() = default;
        loadModelBatch(const char *directory, const char *file, GPPOBJ &target)
            : obj(&target), path(directory), model(file) {}
    };

    void initializeModels();
    void shutdownModels();
    CModelManager &modelLoader();
    CModelManager::Handle requestModel(const std::string &path,
                                       const std::string &file, GPPOBJ &target);
    bool loadModelSync(const std::string &path, const std::string &file,
                       GPPOBJ &target);
    size_t getNumModelsToLoad() const;
    bool basicModelsReady() const;

  private:
    std::unique_ptr<CModelManager> modelManager;
    std::vector<CModelManager::Handle> basicModels;

  public:
    void textureBatchLoad(
        const std::vector<std::pair<std::string, std::string>> texts);
    const gppTexture &loadTexture(const std::string &path,
                                  const std::string &texture,
                                  CLuaH::luaScript *luaScript = nullptr,
                                  bool keepPixels = false);
    static bool loadModelBatchAsync(std::deque<loadModelBatch> &batch);
    bool loadSingleModelAsync(loadModelBatch batch);
    bool loadTextureBatchAsync(std::deque<loadTextureBatch> &batch);
    int loadTextureGetId(const std::string &path, const std::string &texture,
                         CLuaH::luaScript *luaScript = nullptr);
    unsigned int getTextureId(const std::string &name) const noexcept;
    const CTheme &loadThemes(const std::string &theme,
                             CLuaH::luaScript *luaScript = nullptr);
    const std::string addGameCallbacks(const std::string &n, func_t function);

    std::string getCallBackRealName(const std::string &str);

    static void teste(const std::string &name);
    static void startModule(const std::string &name);
    static void startMarathonModule(const std::string &name);
    static void serverModule(const std::string &name);
    static void campaingPlayModule(const std::string &name);
    static void benchmark(const std::string &name);

    void loadAllThemes();

    // Window settings
    static gameWindow getWindowDefaults(bool safeMode = false);
    void settWindowConfigs(const gameWindow &w);

    func_t getCallback(const std::string &str);

    // Render basics
    static void clearScreen();
    void renderFrame();

    static double getWindowProportion();

    // Initer
    static GPPGame &GuitarPP();

    static void setVSyncMode(int mode);

    static void helpMenu(const std::string &name);

    GPPOBJ noteOBJ, triggerBASEOBJ, triggerOBJ, pylmbarOBJ, openNoteOBJ;

    unsigned int strumsTexture3D[6]{};
    unsigned int hopoTexture3D[6]{};
    unsigned int tapTexture3D[6]{};

    unsigned int sbaseTexture3D[6]{};
    unsigned int striggerTexture3D[6]{};
    unsigned int openNoteTexture3D{}, openNoteHOPOTexture3D{},
        openNotePTexture3D{}, openNoteHOPOPTexture3D{};

    unsigned int strumKeys[5]{}, fretOneKey{}, fretTwoKey{};

    unsigned int HUDText, fretboardText, lineText{}, HOPOSText{}, pylmBarText{};

    std::string songToLoad;

    int errorsSound[5]{}, startSound, endSound;

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

    static std::deque<std::string> getDirectory(const char *dir, bool getFiles,
                                                bool getDirectories);
    static std::string caseInsensitiveSearchDir(const char *dir, bool files,
                                                bool directories,
                                                const std::string &searchName);

    static inline bool caseInSensStringCompare(const std::string &str1,
                                               const std::string &str2) {
        return ((str1.size() == str2.size()) &&
                std::equal(str1.begin(), str1.end(), str2.begin(),
                           [](const char &c1, const char &c2) {
                               if (c1 == c2) {
                                   return true;
                               } else if (std::toupper(c1) ==
                                          std::toupper(c2)) {
                                   return true;
                               }

                               return false;
                           }));
    }

    void initialLoad();
    void initialLoad2();

    GPPOBJ testobj; // Active scene model; retained for existing render callers.
    CScene scene;
    std::string backgroundScene = "garage";
    bool prepareScene();
    static int setScene(lua_State *L);
    static int getScene(lua_State *L);

  protected:
    static int registerFunctions(CLuaH::luaState_t &Lstate);
    static int registerGlobals(CLuaH::luaState_t &L);

    static int loadSingleTexture(lua_State *L);
    static int getGameplayRunningTime(lua_State *L);
    static int getDeltaTime(lua_State *L);
    static int getGamePlayPlusState(lua_State *L);

  private:
    std::shared_ptr<CPlayer> mainPlayer;

    static void callbackRenderFrame();
    static void callbackKeys(int key, int scancode, int action, int mods);
    static void callbackJoystick(int jid, int eventId);

    friend struct GPPGameLoadingTestAccess;
    struct loadThreadData {
        std::atomic<bool> processing{};
        // Written by the loader, read by the session before starting gameplay.
        std::atomic<bool> loadFailed{};
        std::atomic<bool> continueThread{};
        std::atomic<bool> loadSong{};
        // prepared is owned by the worker until processing=false is published.
        // Main consumes it before publishing the next loadSong request.
        std::vector<std::unique_ptr<CPlayer>> prepared;
        std::vector<std::string> instruments;
        std::atomic<bool> listEnd{};
        std::atomic<size_t> songID{};

        std::deque<std::string> songsList;

        inline ~loadThreadData() { continueThread = false; }
    };

    static void loadThread(CGamePlay &module, loadThreadData &l);
    static void loadMarathonThread(loadThreadData &l);
    static bool installMarathonSong(CGamePlay &module, loadThreadData &l);

    GPPGame(GPPGame &) = delete;

    bool windowChangedParams{}; // update in case of realtime changes - TODO

    gameWindow windowCFGs; // Actual config
    CGameSettings settings;

    GPPGame();
    ~GPPGame() noexcept;

  public:
    // Main-thread API: snapshots remain valid after the module/player reset.
    const std::vector<SongResult> &getSongResults() const {
        return songResults;
    }

    void clearSongResults() { songResults.clear(); }

    void recordSongResult(SongResult result) {
        songResults.push_back(std::move(result));
        scene.songEnded(songResults.back());
    }

    bool saveSettings();
    const gameWindow &getWindowConfig() const { return windowCFGs; };
    int createWindow();

    static void logError(int code, const std::string &e);
};

#endif
