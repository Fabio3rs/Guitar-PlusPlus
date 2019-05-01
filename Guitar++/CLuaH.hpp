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
#include <memory>

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

		nullinit(const T &l) noexcept : i(l){};
		nullinit() noexcept : i((T)0){};
		nullinit(T l) noexcept : i(l){};
		nullinit(const nullinit<T>&l) noexcept : i(l){};
	};

	/*
	* luaScript can't be duplicated
	* 'cause dctor calls lua_close(luaState)
	*/
	struct CloseLuaState
	{
		void operator() (lua_State* lua) const noexcept
		{
			lua_close(lua);
		}
	};

	using luaState = std::unique_ptr<lua_State, CloseLuaState>;

	static luaState											make_luaState();
	
	struct luaScript
	{
		luaState											luaState;
		bool												runAgain;

		void												*customPtr;
		std::map < std::string, std::string >				savedValues;
		std::string											filePath;
		std::string											fileName;
		std::map < int, int >								callbacks;

		bool												callbacksAdded;

		void unload() noexcept; // clean lua state, containers, etc.

		luaScript &operator=(luaScript &&script) noexcept;
		luaScript &operator=(const luaScript &script) = delete; // No copy constructor!!!

		luaScript clone();

		std::vector<char> dumpBytecode();

		luaScript(const luaScript &L) = delete;
		luaScript(luaScript &&L) noexcept;
		luaScript() noexcept;
		~luaScript() noexcept;
	};

	std::map < std::string, std::map<std::string, luaScript> >				files; /* std::map<pathForScripts, std::map<scriptName, scriptData>> */

	struct luaVarData
	{
		int type;
		std::string str;
		double num;
		int boolean;
		int function;
		int64_t inumber;

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

		luaVarData &operator=(const luaVarData &m) = default;
		luaVarData &operator=(luaVarData &&m) noexcept
		{
			if (this == std::addressof(m)) return *this;

			type = std::move(m.type);
			str = std::move(m.str);
			num = std::move(m.num);
			boolean = std::move(m.boolean);
			function = std::move(m.function);
			inumber = std::move(m.inumber);

			m.type = LUA_TNUMBER;
			m.num = 0.0;

			return *this;
		}

		luaVarData() : type(LUA_TNUMBER), num(0.0) {}
		luaVarData(const luaVarData&) = default;
		luaVarData(luaVarData &&m) noexcept
		{
			if (this == std::addressof(m)) return;

			type = std::move(m.type);
			str = std::move(m.str);
			num = std::move(m.num);
			boolean = std::move(m.boolean);
			function = std::move(m.function);
			inumber = std::move(m.inumber);

			m.type = LUA_TNUMBER;
			m.num = 0.0;
		}
	};
	
	struct luaScriptGlobals;

	// Lua functions custom param wrapper
	class customParam{
		friend luaScriptGlobals;
		luaVarData p;
		std::map<std::string, customParam> tableData;
		void loadTable(lua_State *L, int idx);

	public:
		void loadTableWOPush(lua_State *L);

		void clear() noexcept
		{
			tableData.clear();
			customParam();
		}

		template<class Archive>
		void load(Archive &archive)
		{
			archive(p, tableData);
		}

		template<class Archive>
		void save(Archive &archive) const
		{
			archive(p, tableData);
		}

		std::map<std::string, customParam> &getTableData()
		{
			return tableData;
		}

		int getType() const noexcept
		{
			return p.type;
		}

		const std::string &getString() const noexcept
		{
			return p.str;
		}

		const double getNumber() const noexcept
		{
			return ((p.type == (LUA_TNUMBER | 0xF0000000))? p.inumber : p.num);
		}

		const bool getBoolean() const noexcept
		{
			return p.boolean != 0;
		}

		const int getFunctionRef() const noexcept
		{
			return p.function;
		}

		void set(const std::string &s)
		{
			p.str = s;
			p.type = LUA_TSTRING;
		}

		void set(const char *s)
		{
			p.str = s;
			p.type = LUA_TSTRING;
		}

		void set(double n) noexcept
		{
			p.num = n;
			p.type = LUA_TNUMBER;
		}

		void set(int64_t n) noexcept
		{
			p.inumber = n;
			p.type = LUA_TNUMBER | 0xF0000000;
		}

		void set(bool n) noexcept
		{
			p.boolean = n;
			p.type = LUA_TBOOLEAN;
		}

		void pushToLuaStack(lua_State *L) const
		{
			switch (p.type)
			{
			case LUA_TNIL:
				break;

			case static_cast<int>(static_cast<unsigned int>(LUA_TNUMBER) | 0xF0000000u):
				lua_pushinteger(L, p.inumber);
				break;

			case LUA_TNUMBER:
				lua_pushnumber(L, p.num);
				break;

			case LUA_TBOOLEAN:
				lua_pushboolean(L, p.boolean);
				break;

			case LUA_TSTRING:
				lua_pushstring(L, p.str.c_str());
				break;

			case LUA_TTABLE:
			{
				lua_newtable(L);

				for (auto &t : tableData)
				{
					lua_pushstring(L, t.first.c_str());
					t.second.pushToLuaStack(L);
					lua_settable(L, -3);
				}
			}
				break;

			case LUA_TFUNCTION:
				lua_pushinteger(L, p.function);
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

		void pushToLuaStack(CLuaH::luaState &L) const
		{
			pushToLuaStack(L.get());
		}

		void getFromArgs(lua_State *L, int idx)
		{
			switch (p.type = lua_type(L, idx))
			{
			case LUA_TNIL:
				break;

			case LUA_TNUMBER:
				if (lua_isinteger(L, idx))
				{
					p.type = (LUA_TNUMBER | 0xF0000000);
					p.inumber = lua_tointeger(L, idx);
				}
				else{
					p.num = lua_tonumber(L, idx);
				}

				break;

			case LUA_TBOOLEAN:
				p.boolean = lua_toboolean(L, idx);
				break;

			case LUA_TSTRING:
				p.str = lua_tostring(L, idx);
				break;

			case LUA_TTABLE:
				loadTable(L, idx);
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

		void getFromArgs(CLuaH::luaState &L, int idx) noexcept
		{
			getFromArgs(L.get(), idx);
		}

		customParam() noexcept
		{
			p.type = LUA_TNIL;
			p.num = 0.0;
			p.inumber = 0;
			p.boolean = NULL;
			p.function = NULL;
		}

		customParam(const luaVarData &data) noexcept
		{
			p = data;
		}

		customParam(const std::string &s)
		{
			p.num = 0.0;
			p.boolean = NULL;
			p.function = NULL;
			p.str = s;
			p.type = LUA_TSTRING;
			p.inumber = 0;
		}

		customParam(const char *s)
		{
			p.num = 0.0;
			p.boolean = NULL;
			p.function = NULL;
			p.str = s;
			p.type = LUA_TSTRING;
			p.inumber = 0;
		}

		customParam(double n) noexcept
		{
			p.boolean = NULL;
			p.function = NULL;
			p.num = n;
			p.type = LUA_TNUMBER;
			p.inumber = 0;
		}

		customParam(int n) noexcept
		{
			p.boolean = NULL;
			p.function = NULL;
			p.num = n;
			p.type = (LUA_TNUMBER | 0xF0000000);
			p.inumber = n;
		}

		customParam(int64_t n) noexcept
		{
			p.boolean = NULL;
			p.function = NULL;
			p.num = 0.0;
			p.type = (LUA_TNUMBER | 0xF0000000);
			p.inumber = n;
		}

		customParam(bool n) noexcept
		{
			p.num = 0.0;
			p.function = NULL;
			p.boolean = n;
			p.type = LUA_TBOOLEAN;
			p.inumber = 0;
		}

		customParam &operator=(const customParam&) = default;

		customParam &operator=(customParam &&m) noexcept
		{
			if (this == std::addressof(m)) return *this;

			p = std::move(m.p);
			tableData = std::move(m.tableData);

			return *this;
		}

		customParam(const customParam&) = default;
		customParam(customParam &&m) noexcept
		{
			if (this == std::addressof(m)) return;

			p = std::move(m.p);
			tableData = std::move(m.tableData);
		}
	};

	static void loadGlobalTable(lua_State *L, customParam &tableData);

	bool registerCustomFunctions;

	typedef std::vector<customParam> multiCallBackParams_t;
	typedef std::vector<CLuaH::luaScript> scriptStorage;

	static CLuaH				&Lua();

	inline bool					initSuccess(){ return inited; }

	/*
	* Load all lua scripts from a path
	*/
	bool						loadFiles(const std::string &path);

	bool						loadFilesDequeStorage(const std::string &path, scriptStorage &storage);

	/*
	* Run all scripts in script quere
	*/
	void						runScripts();


	void						runScriptsFromPath(const std::string &path);

	void						runScriptsFromStorage(scriptStorage &storage);

	/*
	* New script and DO NOT add it to quere
	*/
	luaScript					newScript(const std::string &path, const std::string &f);
	luaScript					newScriptR(const std::string &memf, const std::string &name);
	luaScript					newScriptRBuffer(const char *memf, size_t sz, const std::string &name);
	luaScript					newScriptRBuffer(const std::vector<char> &vec, const std::string &name);

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
	void						runEvent(int id);
	void						runEventFromContainer(int id, scriptStorage &storage);
	
	/*
	* Run a especific with parameteres (calls him specifics callbacks)
	*/
	void						runEventWithParams(int id, const multiCallBackParams_t &params);
	void						runEventWithParamsFromContainer(int id, const multiCallBackParams_t &params, scriptStorage &storage);

	/*
	* Run a internal event (calls him specifics callbacks)
	*/
	void						runInternalEvent(luaScript &L, int id) noexcept;

	/*
	* Run a internal with parameteres (calls him specifics callbacks)
	*/
	void						runInternalEventWithParams(luaScript &L, int id, const multiCallBackParams_t &params) noexcept;

	luaScript					&getScript(const std::string &path, const std::string &f)
	{
		return files[path][f];
	}

	/*
	* Get last runned (or running) script
	*/
	luaScript					&getLuaStateScript(lua_State *L);

	static std::string			getGlobalVarAsString(luaScript &l, const std::string &varname);
	static const char*			getGlobalVarAsString(luaScript &l, const char *varname) noexcept;

	void						unloadAll() noexcept;

	int							idForCallbackEvent(const std::string &s);

private:
	static int destroyScriptInstance;

	std::map < std::string, nullinit<int> >									callbacksNamesID;
	//std::vector < luaScript* > lastScript;

	CLuaH(const CLuaH&) = delete;


	/*
	* Run only one script with args (already in Lua Stack)
	*/
	int							runScriptWithArgs(luaScript &lua, int args);

	void catchErrorString(lua_State *L);
	void catchErrorString(const luaScript &L);

	CLuaH() noexcept;
};


#endif