#pragma once
// brmodstudio.forumeiros.com
#ifndef _LUA_HOOKER_CLUAH_HPP_
#define _LUA_HOOKER_CLUAH_HPP_

#include "Lua/lua.hpp"
#include <string>
#include <map>
#include <deque>
#include <cstdint>
#include <iostream>
#include <vector>

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

	struct stringHash
	{
		std::string str;
		uint64_t hash64;
		double gppCalc;

		inline bool operator==(const stringHash &hs) const
		{
			return hash64 == hs.hash64 && gppCalc == hs.gppCalc && str.size() == hs.str.size();
		}

		inline stringHash &operator=(const std::string &s)
		{
			set(s);
			return *this;
		}

		inline bool operator>(const stringHash &hs) const
		{
			return hash64 > hs.hash64 && str.size() > hs.str.size();
		}

		inline bool operator<(const stringHash &hs) const
		{
			return hash64 < hs.hash64 && str.size() < hs.str.size();
		}

		void set(const std::string &s)
		{
			str = s;
			hash64 = ~(0uL);
			static double sqrt12 = sqrt(12);

			static uint64_t keys[255] = {0};
			static bool bkeyssetted = false;

			if (!bkeyssetted)
			{
				uint64_t i = 1;
				for (auto &k : keys)
				{
					k = sqrt12 * 1.0 / (i * 2.0 + 3.0) * pow(3.0, i) * pow(s.size() + 1, rand() % 255);

					k ^= rand();
					k <<= 16;
					k ^= rand();
					k <<= 16;
					k ^= rand();
					k <<= 16;
					k ^= rand();
					k <<= 16;

					k ^= ~(rand() % (s.size() + 1));

					k <<= 8;

					k ^= ~(rand() % (s.size() + 1));

					i *= 3;
				}

				bkeyssetted = true;
			}

			for (int i = 0, size = str.size(); i < size; ++i)
			{
				uint64_t pVal = keys[str[i]];
				std::cout << str[i] << "   " << pVal << std::endl;

				uint64_t sv = (i + 1) * s[i] + i;
				
				hash64 ^= pVal * sv;
				
				hash64 <<= 1;

				gppCalc += str[i];
			}
		}

		inline stringHash(const std::string &s)
		{
			set(s);
			gppCalc = 0;
		}

		inline stringHash()
		{
			hash64 = 0;
			gppCalc = 0;
		}
	};

	/*
	* luaScript can't be duplicated
	* 'cause dctor calls lua_close(luaState)
	*/
	struct luaScript
	{
		lua_State											*luaState;
		bool												runAgain;

		std::map < std::string, std::string >		savedValues;
		std::string											filePath;
		std::string											fileName;
		std::map < std::string, int >				callbacks;
		std::map < std::string, int >				cheats;
		std::map < uintptr_t, int >				hooks;

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

	std::map < std::string, std::map<std::string, luaScript> >		files; /* std::map<pathForScripts, std::map<scriptName, scriptData>> */
	std::map <std::string, callBacksStruct>									callbacks;

	// Lua functions custom param wrapper
	class customParam{
		int type;
		std::string str;
		double num;
		int boolean;
		int function;
		int64_t inumber;

	public:
		template<class Archive>
		void load(Archive &archive)
		{
			archive(type, str, num, boolean, function, inumber);
		}

		template<class Archive>
		void save(Archive &archive) const
		{
			archive(type, str, num, boolean, function, inumber);
		}

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

	bool registerCustomFunctions;

	typedef std::deque<customParam> multiCallBackParams_t;

	static CLuaH				&Lua();

	inline bool					initSuccess(){ return inited; }

	/*
	* Load all lua scripts from a path
	*/
	bool						loadFiles(const std::string &path);

	bool						loadFilesDequeStorage(const std::string &path, std::deque<CLuaH::luaScript> &storage);

	/*
	* Run all scripts in script quere
	*/
	void						runScripts();


	void						runScriptsFromPath(const std::string &path);

	void						runScriptsFromDequeStorage(std::deque<CLuaH::luaScript> &storage);

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
	void						runEvent(const std::string &name);
	void						runCheatEvent(const std::string &name);
	void						runHookEvent(uintptr_t address);
	void						runEventFromDeque(const std::string &name, std::deque<CLuaH::luaScript> &storage);
	
	/*
	* Run a especific with parameteres (calls him specifics callbacks)
	*/
	void						runEventWithParams(const std::string &name, const multiCallBackParams_t &params);
	void						runEventWithParamsFromDeque(const std::string &name, const multiCallBackParams_t &params, std::deque<CLuaH::luaScript> &storage);

	/*
	* Run a internal event (calls him specifics callbacks)
	*/
	void						runInternalEvent(luaScript &L, const std::string &name);

	/*
	* Run a internal with parameteres (calls him specifics callbacks)
	*/
	void						runInternalEventWithParams(luaScript &L, const std::string &name, const multiCallBackParams_t &params);

	luaScript					&getScript(const std::string &path, const std::string &f)
	{
		return files[path][f];
	}

	/*
	* Get last runned (or running) script
	*/
	inline luaScript &getLastScript(){ return *lastScript.back(); }

	static std::string getGlobalVarAsString(luaScript &l, const std::string &varname);
	static const char *getGlobalVarAsString(luaScript &l, const char *varname);

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