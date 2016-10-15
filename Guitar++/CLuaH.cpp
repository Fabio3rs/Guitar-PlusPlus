// LuaHooker.cpp : Defines the exported functions for the DLL application.
//

#include "CLuaH.hpp"
#include "CLuaFunctions.hpp"
#include <string>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include "CLog.h"
#include <dirent.h>

CLuaH &CLuaH::Lua()
{
	static CLuaH L;
	return L;
}

bool CLuaH::loadFiles(const std::string &path)
{
	auto extension_from_filename = [](const std::string &fname)
	{
		size_t s;
		return std::string(((s = fname.find_first_of('.')) != fname.npos) ? (&fname.c_str()[++s]) : (""));
	};

	DIR *direntd = opendir((std::string("./") + path).c_str());
	dirent *rrd = nullptr;

	if (direntd)
	{
		rrd = readdir(direntd);
		while ((rrd = readdir(direntd)) != nullptr)
		{
			if ((rrd->d_type & DT_DIR) == 0 && extension_from_filename(rrd->d_name) == "lua")
			{
				CLog::log() << ("Loading <<" + path + "/" + rrd->d_name + ">>");
				files[path][rrd->d_name] = newScript(path, rrd->d_name);
			}
		}
		closedir(direntd);
	}
	
	return true;
}

CLuaH::luaScript *CLuaH::newScriptInQuere(luaScript &&lua){
	files[lua.filePath][lua.fileName] = std::move(lua);
	return &files[lua.filePath][lua.fileName];
}

CLuaH::luaScript CLuaH::newScriptR(const std::string &memf, const std::string &name){
	static const std::string barra("/");
	auto file_exists = [](const std::string &fileName){
		return std::fstream(fileName).is_open();
	};

	if (memf.size() == 0){
		luaScript lData;

		lData.luaState = nullptr;

		CLog::log() << ("Fail to load CLuaH::newScriptR - " + name);
		return lData;
	}

	luaScript lData;
	lData.luaState = luaL_newstate();
	luaL_openlibs(lData.luaState);
	lData.filePath = "*";
	lData.fileName = "";

	CLuaFunctions::LuaF().registerFunctions(lData.luaState);
	CLuaFunctions::LuaF().registerGlobals(lData.luaState);

	int load_result = luaL_loadbuffer(lData.luaState, memf.c_str(), memf.size(), name.c_str());

	if (load_result != 0){
		CLog::log() << std::string("luaL_loadbuffer(") + std::to_string((unsigned int)lData.luaState) +
			std::string(", ") + std::string(lData.filePath + barra + lData.fileName) +
			std::string(") failed to load with result ") + std::to_string(load_result);

		catchErrorString(lData);
	}
	return lData;
}

CLuaH::luaScript CLuaH::newScript(const std::string &path, const std::string &f){
	static const std::string barra("/");
	auto file_exists = [](const std::string &fileName){
		return std::fstream(fileName).is_open();
	};

	if (!file_exists(std::string(path + barra + f))){
		luaScript lData;

		lData.luaState = nullptr;

		CLog::log() << std::string("Fail to load ") + std::string(path + barra + f);
		return lData;
	}

	luaScript lData;
	lData.luaState = luaL_newstate();
	luaL_openlibs(lData.luaState);
	lData.filePath = path;
	lData.fileName = f;

	CLuaFunctions::LuaF().registerFunctions(lData.luaState);
	CLuaFunctions::LuaF().registerGlobals(lData.luaState);

	int load_result = luaL_loadfile(lData.luaState, std::string(lData.filePath + barra + lData.fileName).c_str());

	if (load_result != 0){
		CLog::log() << std::string("luaL_loadfile(") + std::to_string((unsigned int)lData.luaState) +
			std::string(", ") + std::string(lData.filePath + barra + lData.fileName) +
			std::string(") failed to load with result ") + std::to_string(load_result);

		catchErrorString(lData);
	}
	return lData;
}

void CLuaH::catchErrorString(lua_State *L){
	const char *s = lua_tostring(L, -1);
	if (s == NULL) s = "Unrecognized Lua error";

	CLog::log() << s;
}

void CLuaH::catchErrorString(const luaScript &L){
	static const std::string barra("/");
	const char *s = lua_tostring(L.luaState, -1);
	if (s == NULL) s = "Unrecognized Lua error";

	std::string errorStr = std::string(L.filePath + barra + L.fileName) + std::string(" - ") + s;

	CLog::log() << errorStr;
}

int CLuaH::runScript(luaScript &lua){
	static const std::string barra("/");

	if (lua.luaState){
		lastScript.push_back(&lua);
		auto pcallr = lua_pcall(lua.luaState, 0, LUA_MULTRET, 0);
		lastScript.pop_back();
		return pcallr;
	}

	return false;
}

int CLuaH::runScript(const std::string &path, const std::string &f)
{
	return runScript(files[path][f]);
}

/*
* Run only one script with args (already in Lua Stack)
*/
int CLuaH::runScriptWithArgs(luaScript &lua, int args){
	static const std::string barra("/");

	if (lua.luaState){
		lastScript.push_back(&lua);
		auto pcallr = lua_pcall(lua.luaState, args, LUA_MULTRET, 0);
		lastScript.pop_back();
		return pcallr;
	}

	return false;
}

void CLuaH::runEvent(std::string name)
{
	static const std::string barra("/");

	for (auto &pathScripts : Lua().files)
	{
		for (auto &scripts : pathScripts.second)
		{
			int luaindex = 0;

			if (scripts.second.callbacksAdded && (luaindex = scripts.second.callbacks[name]) != 0){
				lua_rawgeti(scripts.second.luaState, LUA_REGISTRYINDEX, luaindex);
				if (runScript(scripts.second) != 0)
					catchErrorString(scripts.second);
			}
		}
	}
}

void CLuaH::runHookEvent(uintptr_t address){
	static const std::string barra("/");
	for (auto &pathScripts : Lua().files)
	{
		for (auto &scripts : pathScripts.second)
		{
			int luaindex = 0;

			if (scripts.second.hooksAdded && (luaindex = scripts.second.hooks[address]) != 0){
				lua_rawgeti(scripts.second.luaState, LUA_REGISTRYINDEX, luaindex);
				if (runScript(scripts.second) != 0)
					catchErrorString(scripts.second);
			}
		}
	}
}


void CLuaH::runCheatEvent(std::string name){
	static const std::string barra("/");
	if (name.size() < 1)
	{
		return;
	}

	for (auto &pathScripts : Lua().files)
	{
		for (auto &scripts : pathScripts.second)
		{
			int luaindex = 0;

			if (scripts.second.cheatsAdded)
			{
				for (auto &cname : scripts.second.cheats)
				{
					if ((luaindex = cname.second) != 0)
					{
						int size = cname.first.size();

						bool canExecuteCallback = true;

						for (int i = size - 1; i >= 0; --i)
						{
							if (name[i] != cname.first[size - (i + 1)])
							{
								canExecuteCallback = false;
								break;
							}
						}

						if (canExecuteCallback)
						{
							lua_rawgeti(scripts.second.luaState, LUA_REGISTRYINDEX, luaindex);
							if (runScript(scripts.second) != 0)
							{
								catchErrorString(scripts.second);
							}
							else
							{
								/*char *buffer = (char*)0x969110;

								*buffer = 0;*/
							}
						}
					}
				}
			}
		}
	}
}

/*
* Run a especific with parameteres (calls him specifics callbacks)
*/
void CLuaH::runEventWithParams(const std::string &name, const multiCallBackParams_t &params){
	static const std::string barra("/");
	for (auto &pathScripts : Lua().files)
	{
		for (auto &scripts : pathScripts.second)
		{
			if (scripts.second.callbacksAdded && scripts.second.callbacks[name] != 0){
				lua_rawgeti(scripts.second.luaState, LUA_REGISTRYINDEX, scripts.second.callbacks[name]);

				for (auto &p : params){
					p.pushToLuaStack(scripts.second.luaState);
				}

				if (runScriptWithArgs(scripts.second, params.size()) != 0){
					std::string errors = "Event name ";
					errors += name;
					errors += " num params: ";
					errors += std::to_string(params.size());
					errors += " data: ";

					for (auto &p : params){
						switch (p.getType())
						{
						case LUA_TNIL:
							errors += " <null> ";
							break;

						case LUA_TNUMBER:
							errors += " Number<";
							errors += std::to_string(p.getNumber());
							errors += "> ";
							break;

						case LUA_TBOOLEAN:
							errors += ((p.getBoolean() != 0) ? " Boolean<True> " : " Boolean<False> ");
							break;

						case LUA_TSTRING:
							errors += " String<";
							errors += p.getString();
							errors += "> ";
							break;

						case LUA_TTABLE:
							errors += " <Get LUA_TTABLE: **TODO**> ";
							break;

						case LUA_TFUNCTION:
							errors += " <Get LUA_TFUNCTION: **TODO**> ";
							break;

						case LUA_TUSERDATA:
							errors += " <Get LUA_TUSERDATA: **TODO**> ";
							break;

						case LUA_TTHREAD:
							errors += " <Get LUA_TTHREAD: **TODO**> ";
							break;

						case LUA_TLIGHTUSERDATA:
							errors += " <Get LUA_TLIGHTUSERDATA: **TODO**> ";
							break;

						default:
							errors += " <unknow argment type> ";
							break;
						}
					}

					CLog::log() << errors;
					catchErrorString(scripts.second);
				}
			}
		}
	}
}

void CLuaH::runScriptsFromPath(const std::string &path)
{
	for (auto &pathScripts : Lua().files[path])
	{
		runScript(pathScripts.second);
	}
}

void CLuaH::runScripts(){
	static const std::string barra("/");
	for (auto &pathScripts : Lua().files)
	{
		for (auto &scripts : pathScripts.second)
		{
			runScript(scripts.second);
		}
	}
}

/*
* Run a internal event (calls him specifics callbacks)
*/
void CLuaH::runInternalEvent(luaScript &L, std::string name)
{
	if (L.callbacksAdded && L.callbacks[name] != 0){
		lua_rawgeti(L.luaState, LUA_REGISTRYINDEX, L.callbacks[name]);
		if (runScript(L) != 0)
			catchErrorString(L);
	}
}

CLuaH::luaScript::luaScript(){
	luaState = nullptr;
	runAgain = true;
	cheatsAdded = false;
	callbacksAdded = false;
	hooksAdded = false;
}

CLuaH::luaScript &CLuaH::luaScript::operator=(luaScript &&script){
	luaState = script.luaState;

	savedValues = std::move(script.savedValues);
	filePath = std::move(script.filePath);
	fileName = std::move(script.fileName);
	callbacks = std::move(script.callbacks);

	script.luaState = nullptr;

	return *this;
}

void CLuaH::luaScript::unload(){
	if (luaState != nullptr){
		CLuaH::Lua().runInternalEvent(*this, "destroyScriptInstance");

		lua_close(luaState);
		luaState = nullptr;
	}

	cheatsAdded = false;
	callbacksAdded = false;
	hooksAdded = false;
	savedValues.clear();
	filePath.clear();
	fileName.clear();
	callbacks.clear();
}

/*
* luaScript can't be duplicated
* 'cause dctor calls lua_close(luaState)
* copy ctor works like move operator
*/
CLuaH::luaScript::luaScript(luaScript &L){
	luaState = L.luaState;

	savedValues = std::move(L.savedValues);
	filePath = std::move(L.filePath);
	fileName = std::move(L.fileName);
	callbacks = std::move(L.callbacks);

	L.luaState = nullptr;

	cheatsAdded = L.cheatsAdded;
	callbacksAdded = L.callbacksAdded;
	hooksAdded = L.hooksAdded;
}

std::string CLuaH::getGlobalVarAsString(luaScript &l, const std::string &varname)
{
	lua_getglobal(l.luaState, varname.c_str());

	return lua_tostring(l.luaState, -1);
}

CLuaH::luaScript::~luaScript(){
	if (luaState != nullptr){
		CLuaH::Lua().runInternalEvent(*this, "destroyScriptInstance");

		lua_close(luaState);
		luaState = nullptr;
	}
}

void CLuaH::unloadAll(){
	files.clear();

}

CLuaH::CLuaH()
{
	inited = loadFiles("LuaScripts");

	runScripts();
}


