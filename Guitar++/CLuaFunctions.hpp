#pragma once
#ifndef _LUA_HOOKER_CLUAFUNCTIONS_HPP_
#define _LUA_HOOKER_CLUAFUNCTIONS_HPP_

#include <memory>
#include <map>
#include "CLuaH.hpp"
#include <string>
#include <deque>
#include <functional>
#include <memory>
#include <algorithm>

class CLuaFunctions{
	std::deque < std::function<int(lua_State*)> > registerFunctionsAPICBs;
	std::deque < std::function<int(lua_State*)> > registerGlobalsAPICBs;
	std::deque < std::function<void(void)> > frameUpdateAPICBs;

public:
	static CLuaFunctions &LuaF();

	class LuaParams
	{
		lua_State *L;
		int num_params, ret, stck, fail_bit;

	public:

		LuaParams &operator<<(const std::string &param);
		LuaParams &operator<<(double param);
		LuaParams &operator<<(int param);
		LuaParams &operator<<(int64_t param);
		LuaParams &operator<<(bool param);
		LuaParams &operator<<(size_t param);

		LuaParams &operator>>(std::string &param);
		LuaParams &operator>>(double &param);
		LuaParams &operator>>(int &param);
		LuaParams &operator>>(int64_t &param);
		LuaParams &operator>>(void *&param);
		LuaParams &operator>>(bool &param);

		template<class T>
		inline LuaParams &operator>>(T &param){
			if (stck <= num_params){
				param = (T)(lua_tointeger(L, stck));
				++stck;
			}
			else
			{
				fail_bit = 1;
			}

			return *this;
		}

		int getNumParams();
		int rtn();

		bool fail();

		LuaParams(lua_State *state);
	};

	class GameVariables{
		enum vartype{ nonev, integer64, integerv, uintegervar, doublevar, stringvar, booleanvar };

		struct vard
		{
			vartype t;
			void *ptr;

			inline vard()
			{
				t = nonev;
				ptr = nullptr;
			}
		};

		std::map <std::string, vard> vars;

	public:
		void pushVar(const std::string &name, int &var);
		void pushVar(const std::string &name, int64_t &var);
		void pushVar(const std::string &name, double &var);
		void pushVar(const std::string &name, std::string &var);
		void pushVar(const std::string &name, bool &var);

		void setVar(const std::string &name, int value);
		void setVar(const std::string &name, int64_t value);
		void setVar(const std::string &name, double value);
		void setVar(const std::string &name, std::string value);
		void setVar(const std::string &name, bool value);

		void pushToLuaStack(const std::string &name, lua_State *L);

		void setVar(const std::string &name, lua_State *L, int stackIDX);

		void removeVar(const std::string &name);

		static GameVariables &gv();

	private:
		GameVariables(GameVariables&) = delete;
		GameVariables();
	};

	// Custom lua functions

	/*
	* Set Window/game configurations
	*/
	static int setConfigs(lua_State *L);

	/*
	* Don't run the script again... Deprecated futurely?
	*/
	static int doNotRunAgain(lua_State *L);

	/*
	* Set a lua function as callback for a event
	*/
	static int setCallBackToEvent(lua_State *L);

	/*
	* CFonts/Draw text
	*/
	static int drawTextOnScreen(lua_State *L);

	/*
	* New menu
	*/
	static int newMenu(lua_State *L);

	/*
	* New menu option
	*/
	static int newMenuOption(lua_State *L);

	/*
	* 
	*/
	static int getMainMenuName(lua_State *L);

	/*
	* Output a string in game log
	*/
	static int printTolog(lua_State *L);

	/*
	* 
	*/
	static int getMenuOptionName(lua_State *L);

	/*
	*
	*/
	static int setMenuBackgroundTexture(lua_State *L);

	/*
	*
	*/
	static int assingMenuToOtherMenuOption(lua_State *L);

	/*
	*
	*/
	static int assingGameFunctionToMenuOption(lua_State *L);

	/*
	*
	*/
	static int getNumOfMenuOptions(lua_State *L);

	/*
	*
	*/
	static int getActualMenu(lua_State *L);

	/*
	*
	*/
	static int getGameCallback(lua_State *L);

	/*
	* Load a texture (load file & load opengl texture)
	*/
	static int loadTexture(lua_State *L);


	/*
	* Adds Texture to font and create a font instance if it doesn't exists
	*/
	static int addTextureToFont(lua_State *L);


	/*
	*
	*/
	static int newGamePlayModule(lua_State *L);


	static int loadSound(lua_State *L);
	static int loadSoundDecode(lua_State *L);
	static int playSound(lua_State *L);
	static int pauseSound(lua_State *L);
	static int stopSound(lua_State *L);
	static int setSoundTime(lua_State *L);
	static int setSoundVolume(lua_State *L);
	static int getSoundVolume(lua_State *L);
	static int getSoundTime(lua_State *L);
	static int releaseSound(lua_State *L);
	static int setSoundFlags(lua_State *L);
	static int setSoundAttribute(lua_State *L);

	static int getBassError(lua_State *L);

	static int setGameVar(lua_State *L);
	static int getGameVar(lua_State *L);

	static int printGlobals(lua_State *L);

	/*
	* Register custom functions lua state
	*/
	void registerFunctions(lua_State *L);

	/*
	* Register default game globals
	*/
	void registerGlobals(lua_State *L);

	void registerLuaFuncsAPI(std::function<int(lua_State*)> fun);
	void registerLuaGlobalsAPI(std::function<int(lua_State*)> fun);
	void registerFrameUpdateAPI(std::function<void(void)> fun);

private:
	CLuaFunctions();
	CLuaFunctions(CLuaFunctions&) = delete;
	~CLuaFunctions() = default;
};

#endif