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

		bool loaded;
		std::string name, file;

		std::map < std::string, std::string > entries;

	public:

		CLanguage(CLuaH::luaScript &ls);
		CLanguage();
	};

	CLanguageManager();

	std::map < std::string, CLanguage > langs;

public:

	std::string getText(const std::string &lang, const std::string &entry);

	static CLanguageManager &langMGR();
};


#endif