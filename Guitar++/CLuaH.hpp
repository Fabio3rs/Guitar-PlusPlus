#pragma once
#ifndef _LUA_HOOKER_CLUAH_HPP_
#define _LUA_HOOKER_CLUAH_HPP_

#include "Lua/lua.hpp"
#include <string>
#include <map>
#include <deque>
#include <unordered_map>

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
		std::unordered_map <std::string, unsigned int>		textureList; // useless?

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

	public:
		int getType() const{
			return type;
		}

		const std::string &getString() const{
			return str;
		}

		const double getNumber() const{
			return num;
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
			num = n;
			type = LUA_TNUMBER;
		}

		void set(bool n){
			boolean = n;
			type = LUA_TBOOLEAN;
		}

		void pushToLuaStack(lua_State *L){
			switch (type)
			{
			case LUA_TNIL:
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
				num = lua_tonumber(L, idx);
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
			boolean = NULL;
			function = NULL;
		}

		customParam(const std::string &s){
			num = 0.0;
			boolean = NULL;
			function = NULL;
			str = s;
			type = LUA_TSTRING;
		}

		customParam(const char *s){
			num = 0.0;
			boolean = NULL;
			function = NULL;
			str = s;
			type = LUA_TSTRING;
		}

		customParam(double n){
			boolean = NULL;
			function = NULL;
			num = n;
			type = LUA_TNUMBER;
		}

		customParam(int n){
			boolean = NULL;
			function = NULL;
			num = n;
			type = LUA_TNUMBER;
		}

		customParam(bool n){
			num = 0.0;
			function = NULL;
			boolean = n;
			type = LUA_TBOOLEAN;
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

	/*
	* New script and DO NOT add it to quere
	*/
	luaScript					newScript(const std::string &path, const std::string &f);

	/*
	* New script and add it to quere
	*/
	void						newScriptInQuere(luaScript &&lua);

	/*
	* Run only one script
	*/
	int							runScript(luaScript &lua);

	/*
	* Run a especific event (calls him specifics callbacks)
	*/
	void						runEvent(std::string name);
	
	/*
	* Run a especific with parameteres (calls him specifics callbacks)
	*/
	void						runEventWithParams(const std::string &name, multiCallBackParams_t &params);

	/*
	* Run a internal event (calls him specifics callbacks)
	*/
	void						runInternalEvent(luaScript &L, std::string name);

	/*
	* Run a internal with parameteres (calls him specifics callbacks)
	*/
	void						runinternalEventWithParams(luaScript &L, const std::string &name, multiCallBackParams_t &params);

	/*
	* Get last runned (or running) script
	*/
	inline luaScript &getLastScript(){ return *lastScript; }

private:
	CLuaH(const CLuaH&) = delete;


	/*
	* Run only one script with args (already in Lua Stack)
	*/
	int							runScriptWithArgs(luaScript &lua, int args);

	void catchErrorString(lua_State *L);
	void catchErrorString(const luaScript &L);

	luaScript *lastScript;

	CLuaH();
};


#endif