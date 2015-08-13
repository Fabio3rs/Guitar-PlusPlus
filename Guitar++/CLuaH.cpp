// LuaHooker.cpp : Defines the exported functions for the DLL application.
//

#include "CLuaH.hpp"
#include "CLuaFunctions.hpp"
#include <Windows.h>
#include <string>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include "CLog.h"

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
		return std::string((s = fname.find_last_of('.') != fname.npos)? &fname.c_str()[++s] : "");
	};

	// TODO: change to STD FileSystem

	HANDLE hFind;
	WIN32_FIND_DATA data;

	hFind = FindFirstFile((std::string("./") + path + "/*.lua").c_str(), &data);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				luaScript lData = newScript(path, data.cFileName);
				
				files[lData.filePath][lData.fileName] = lData;
			}
		} while (FindNextFile(hFind, &data));
		FindClose(hFind);
	}
	
	return true;
}

void CLuaH::newScriptInQuere(const luaScript &lua){
	files[lua.filePath][lua.fileName] = lua;
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
			std::string(") fail to load with result ") + std::to_string(load_result);

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
		lastScript = &lua;
		return lua_pcall(lua.luaState, 0, LUA_MULTRET, 0);
	}

	return false;
}

/*
* Run only one script with args (already in Lua Stack)
*/
int CLuaH::runScriptWithArgs(luaScript &lua, int args){
	static const std::string barra("/");

	if (lua.luaState){
		lastScript = &lua;
		return lua_pcall(lua.luaState, args, LUA_MULTRET, 0);
	}

	return false;
}

void CLuaH::runEvent(std::string name){
	static const std::string barra("/");
	for (auto &pathScripts : Lua().files)
	{
		for (auto &scripts : pathScripts.second)
		{
			if (scripts.second.callbacks[name] != 0){
				lua_rawgeti(scripts.second.luaState, LUA_REGISTRYINDEX, scripts.second.callbacks[name]);
				if (runScript(scripts.second) != 0)
					catchErrorString(scripts.second);
			}
		}
	}
}

/*
* Run a especific with parameteres (calls him specifics callbacks)
*/
void CLuaH::runEventWithParams(const std::string &name, multiCallBackParams_t &params){
	static const std::string barra("/");
	for (auto &pathScripts : Lua().files)
	{
		for (auto &scripts : pathScripts.second)
		{
			if (scripts.second.callbacks[name] != 0){
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

CLuaH::luaScript::luaScript(){
	luaState = nullptr;
	runAgain = true;
}

CLuaH::CLuaH()
{
	lastScript = nullptr;
	inited = true;
}


