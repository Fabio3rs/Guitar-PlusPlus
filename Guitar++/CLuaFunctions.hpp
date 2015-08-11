#pragma once
#ifndef _LUA_HOOKER_CLUAFUNCTIONS_HPP_
#define _LUA_HOOKER_CLUAFUNCTIONS_HPP_

#include <memory>
#include <unordered_map>
#include "CLuaH.hpp"
#include <map>
#include <string>

class CLuaFunctions{
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
		LuaParams &operator<<(bool param);
		LuaParams &operator<<(size_t param);

		LuaParams &operator>>(std::string &param);
		LuaParams &operator>>(double &param);
		LuaParams &operator>>(int &param);
		LuaParams &operator>>(void *&param);
		LuaParams &operator>>(bool &param);

		template<class T>
		inline LuaParams &operator>>(T &param){
			if (stck <= num_params){
				param = (T)(lua_tounsigned(L, stck));
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
	static int getNumOfMenuOptions(lua_State *L);


	/*
	* Register custom functions lua state
	*/
	void registerFunctions(lua_State *L);

	CLuaFunctions(CLuaFunctions&) = delete;

private:
	CLuaFunctions();
};

#endif