#pragma once
// brmodstudio.forumeiros.com
#ifndef _LUA_HOOKER_CLUAH_HPP_
#define _LUA_HOOKER_CLUAH_HPP_

#include "Lua/lua.hpp"
#include <string>
#include <map>
#include <deque>
#include <unordered_map>
#include <cstdint>

class CLuaH
{
	bool inited;

public:
	template<class T>
	class nullinit{
		T i;
	public:
		T &operator()(){ return i; }

		nullinit<T> &operator=(const T &l){ i = l; return *this; }
		nullinit<T> &operator=(nullinit<T> &l){ i = l; return *this; }

		nullinit(const T &l) : i(l){};
		nullinit() : i((T)0){};
		nullinit(T l) : i(l){};
		nullinit(const nullinit<T>&l) : i(l){};
	};

	/*
	* luaScript can't be duplicated
	* 'cause dctor calls lua_close(luaState)
	*/
	struct luaScript
	{
		lua_State											*luaState;
		bool												runAgain;

		std::unordered_map < std::string, std::string >		savedValues;
		std::string											filePath;
		std::string											fileName;
		std::unordered_map < std::string, int >				callbacks;
		std::unordered_map < std::string, int >				cheats;
		std::unordered_map < uintptr_t, int >				hooks;

		bool												cheatsAdded;
		bool												callbacksAdded;
		bool												hooksAdded;

		void unload(); // clean lua state, containers, etc.

		luaScript &operator=(luaScript &&script);
		luaScript &operator=(const luaScript &script) = delete; // No copy constructor!!!

		luaScript(luaScript &L); // Works like move operator
		luaScript();
		~luaScript();
	};

	struct callBacksStruct{
		std::string name;
	};

	std::unordered_map < std::string, std::unordered_map<std::string, luaScript> >		files; /* std::unordered_map<pathForScripts, std::unordered_map<scriptName, scriptData>> */
	std::unordered_map <std::string, callBacksStruct>									callbacks;

	// Lua functions custom param wrapper
	class customParam{
		int type;
		std::string str;
		double num;
		int boolean;
		int function;
		int64_t inumber;

	public:
		int getType() const{
			return type;
		}

		const std::string &getString() const{
			return str;
		}

		const double getNumber() const{
			return ((type == (LUA_TNUMBER | 0xF0000000))? inumber : num);
		}

		const bool getBoolean() const{
			return boolean != 0;
		}

		const int getFunctionRef() const{
			return function;
		}

		void set(const std::string &s){
			str = s;
			type = LUA_TSTRING;
		}

		void set(const char *s){
			str = s;
			type = LUA_TSTRING;
		}

		void set(double n){
			num = n;
			type = LUA_TNUMBER;
		}

		void set(int n){
			inumber = num = n;
			type = LUA_TNUMBER | 0xF0000000;
		}

		void set(bool n){
			boolean = n;
			type = LUA_TBOOLEAN;
		}

		void pushToLuaStack(lua_State *L) const{
			switch (type)
			{
			case LUA_TNIL:
				break;

			case (LUA_TNUMBER | 0xF0000000):
				lua_pushinteger(L, inumber);
				break;

			case LUA_TNUMBER:
				lua_pushnumber(L, num);
				break;

			case LUA_TBOOLEAN:
				lua_pushboolean(L, boolean);
				break;

			case LUA_TSTRING:
				lua_pushstring(L, str.c_str());
				break;

			case LUA_TTABLE:
				break;

			case LUA_TFUNCTION:
				lua_pushinteger(L, function);
				break;

			case LUA_TUSERDATA:
				break;

			case LUA_TTHREAD:
				break;

			case LUA_TLIGHTUSERDATA:
				break;

			default:
				break;
			}
		}

		void getFromArgs(lua_State *L, int idx){
			switch (type = lua_type(L, idx))
			{
			case LUA_TNIL:
				break;

			case LUA_TNUMBER:
				if (lua_isinteger(L, idx))
				{
					type = (LUA_TNUMBER | 0xF0000000);
					inumber = num = lua_tointeger(L, idx);
				}
				else{
					num = lua_tonumber(L, idx);
				}

				break;

			case LUA_TBOOLEAN:
				boolean = lua_toboolean(L, idx);
				break;

			case LUA_TSTRING:
				str = lua_tostring(L, idx);
				break;

			case LUA_TTABLE:
				break;

			case LUA_TFUNCTION:
				//lua_pushinteger(L, function);
				break;

			case LUA_TUSERDATA:
				break;

			case LUA_TTHREAD:
				break;

			case LUA_TLIGHTUSERDATA:
				break;

			default:
				break;
			}
		}

		customParam(){
			type = LUA_TNIL;
			num = 0.0;
			inumber = 0;
			boolean = NULL;
			function = NULL;
		}

		customParam(const std::string &s){
			num = 0.0;
			boolean = NULL;
			function = NULL;
			str = s;
			type = LUA_TSTRING;
			inumber = 0;
		}

		customParam(const char *s){
			num = 0.0;
			boolean = NULL;
			function = NULL;
			str = s;
			type = LUA_TSTRING;
			inumber = 0;
		}

		customParam(double n){
			boolean = NULL;
			function = NULL;
			num = n;
			type = LUA_TNUMBER;
			inumber = 0;
		}

		customParam(int n){
			boolean = NULL;
			function = NULL;
			num = n;
			type = (LUA_TNUMBER | 0xF0000000);
			inumber = n;
		}

		customParam(int64_t n){
			boolean = NULL;
			function = NULL;
			num = n;
			type = (LUA_TNUMBER | 0xF0000000);
			inumber = n;
		}

		customParam(bool n){
			num = 0.0;
			function = NULL;
			boolean = n;
			type = LUA_TBOOLEAN;
			inumber = 0;
		}
	};

	typedef std::deque<customParam> multiCallBackParams_t;

	static CLuaH				&Lua();

	inline bool					initSuccess(){ return inited; }

	/*
	* Load all lua scripts from a path
	*/
	bool						loadFiles(const std::string &path);

	/*
	* Run all scripts in script quere
	*/
	void						runScripts();


	void						runScriptsFromPath(const std::string &path);

	/*
	* New script and DO NOT add it to quere
	*/
	luaScript					newScript(const std::string &path, const std::string &f);
	luaScript					newScriptR(const std::string &memf, const std::string &name);

	/*
	* New script and add it to quere
	*/
	luaScript					*newScriptInQuere(luaScript &&lua);

	/*
	* Run only one script
	*/
	int							runScript(luaScript &lua);
	int							runScript(const std::string &path, const std::string &f);

	/*
	* Run a especific event (calls him specifics callbacks)
	*/
	void						runEvent(std::string name);
	void						runCheatEvent(std::string name);
	void						runHookEvent(uintptr_t address);
	
	/*
	* Run a especific with parameteres (calls him specifics callbacks)
	*/
	void						runEventWithParams(const std::string &name, const multiCallBackParams_t &params);

	/*
	* Run a internal event (calls him specifics callbacks)
	*/
	void						runInternalEvent(luaScript &L, std::string name);

	/*
	* Run a internal with parameteres (calls him specifics callbacks)
	*/
	void						runinternalEventWithParams(luaScript &L, const std::string &name, multiCallBackParams_t &params);

	luaScript					&getScript(const std::string &path, const std::string &f)
	{
		return files[path][f];
	}

	/*
	* Get last runned (or running) script
	*/
	inline luaScript &getLastScript(){ return *lastScript.back(); }

	static std::string getGlobalVarAsString(luaScript &l, const std::string &varname);

	void unloadAll();

private:
	std::vector < luaScript* > lastScript;

	CLuaH(const CLuaH&) = delete;


	/*
	* Run only one script with args (already in Lua Stack)
	*/
	int							runScriptWithArgs(luaScript &lua, int args);

	void catchErrorString(lua_State *L);
	void catchErrorString(const luaScript &L);

	CLuaH();
};


#endif