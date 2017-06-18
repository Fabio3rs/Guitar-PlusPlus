#pragma once
#ifndef _GUITARPP_CLANGUAGEMANAGER_H_
#define _GUITARPP_CLANGUAGEMANAGER_H_
#include "CSaveSystem.h"
#include "CLuaH.hpp"
#include <map>
#include <string>

class CLanguageManager
{
	class CLanguage
	{
		friend CLanguageManager;
		std::deque<CLuaH::luaScript*> luaf;

		bool loaded;
		std::string name, path;

		std::map < std::string, std::string > entries;

	public:
		void loadFrom(CLuaH::luaScript &ls);

		CLanguage(const std::string &path);
		CLanguage();
	};

	CLanguageManager();

	std::map < std::string, CLanguage > langs;

protected:
	static int translateString(lua_State *L);
	static int registerFunctions(lua_State *L);
	static int registerGlobals(lua_State *L);

public:
	std::string usingLang;

	std::string getLanguageName(const std::string &lang);
	std::string &getText(const std::string &lang, const std::string &entry);
	std::string getTextDirectFromFile(const std::string &lang, const std::string &entry);

	static CLanguageManager &langMGR();
};


#endif