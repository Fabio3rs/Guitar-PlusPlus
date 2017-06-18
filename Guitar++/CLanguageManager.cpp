#include "CLanguageManager.h"
#include "CLuaFunctions.hpp"
#include <iostream>
#include <dirent.h>
#include "CLog.h"

void PrintTable(lua_State *L)
{
    lua_pushnil(L);

	int index = 0;

    while(index = lua_next(L, -2) != 0)
    {
        if(lua_isstring(L, -1))
          printf("%s = %s\n", lua_tostring(L, -2), lua_tostring(L, -1));
        else if(lua_isnumber(L, -1))
          printf("%s = %d\n", lua_tostring(L, -2), lua_tonumber(L, -1));
        else if(lua_istable(L, -1))
          PrintTable(L);

        lua_pop(L, 1);
    }
}

CLanguageManager::CLanguage::CLanguage(const std::string &path)
{
	/*entries["menuPlayTitle"] = CLuaH::getGlobalVarAsString(ls, "menuPlayTitle");
	entries["menuOptionsTitle"] = CLuaH::getGlobalVarAsString(ls, "menuOptionsTitle");
	entries["menuExtrasTitle"] = CLuaH::getGlobalVarAsString(ls, "menuExtrasTitle");
	*/
	auto &lua = CLuaH::Lua();
	const std::string langPath = "data/languages/" + path;

	lua.registerCustomFunctions = false;

	if (lua.loadFiles(langPath))
	{
		lua.runScriptsFromPath(langPath);
		for (auto &langscript : lua.files[langPath])
		{
			std::string name = langscript.first;
			size_t p = name.find_first_of(".");
			if (p != std::string::npos)
			{
				name.resize(p);
			}

			loadFrom(langscript.second);
		}
	}

	lua.registerCustomFunctions = true;

	loaded = true;
}

void CLanguageManager::CLanguage::loadFrom(CLuaH::luaScript &ls)
{
	luaf.push_back(&ls);

	std::string tmpName = CLuaH::getGlobalVarAsString(ls, "langName");

	if (tmpName.size() > 0)
		name = tmpName;

	lua_State *L = ls.luaState;

	lua_pushglobaltable(L);
	lua_pushnil(L);

	int index = -2;

	while (index = lua_next(L, -2))
	{
		//std::cout << lua_tostring(L, -2) << " " << lua_isstring(L, -1) << std::endl;

		if (lua_isstring(L, -1))
		{
			entries[lua_tostring(L, -2)] = lua_tostring(L, -1);
		}

		lua_pop(L, 1);
	}

	lua_pop(L, 1);

	loaded = true;
}

CLanguageManager::CLanguage::CLanguage()
{
	loaded = false;
}

std::string CLanguageManager::getLanguageName(const std::string &lang)
{
	return langs[lang].name;
}

std::string CLanguageManager::getTextDirectFromFile(const std::string &lang, const std::string &entry)
{
	/*if (langs[lang].luaf == nullptr)
		return "Lang file " + lang + " not loaded";

	return CLuaH::getGlobalVarAsString(*langs[lang].luaf, entry);*/

	return "TODO";
}

std::string &CLanguageManager::getText(const std::string &lang, const std::string &entry)
{
	return langs[lang].entries[entry];
}

CLanguageManager::CLanguageManager()
{
	usingLang = "PT-BR";
	CLuaFunctions::LuaF().registerLuaFuncsAPI(registerFunctions);
	CLuaFunctions::LuaF().registerLuaFuncsAPI(registerGlobals);

	auto &lua = CLuaH::Lua();
	const std::string langPath = "./data/languages";

	lua.registerCustomFunctions = false;

	DIR *direntd = opendir(langPath.c_str());
	dirent *rrd = nullptr;

	if (direntd)
	{
		rrd = readdir(direntd);
		while ((rrd = readdir(direntd)) != nullptr)
		{
			std::string dname = rrd->d_name;
			if ((rrd->d_type & DT_DIR) != 0 && dname != "." && dname != "..")
			{
				CLog::log() << ("Loading <<" + langPath + "/" + dname + ">>");
				
				langs[rrd->d_name] = CLanguage(dname);
			}
		}
		closedir(direntd);
	}

	lua.registerCustomFunctions = true;
}

int CLanguageManager::translateString(lua_State * L)
{
	CLuaFunctions::LuaParams p(L);

	if (p.getNumParams() > 1)
	{
		std::string key, usingLangL = langMGR().usingLang;

		p >> key;

		if (p.getNumParams() == 2)
			p >> usingLangL;

		p << langMGR().getText(usingLangL, key);
	}
	else
	{
		p << "null";
	}

	return p.rtn();
}

int CLanguageManager::registerFunctions(lua_State *L)
{
	lua_register(L, "translateString", translateString);

	return 0;
}

int CLanguageManager::registerGlobals(lua_State *L)
{
	return 0;
}

CLanguageManager &CLanguageManager::langMGR()
{
	static CLanguageManager mgr;
	return mgr;
}


