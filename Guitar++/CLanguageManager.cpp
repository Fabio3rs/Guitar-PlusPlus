#include "CLanguageManager.h"
#include <iostream>

CLanguageManager::CLanguage::CLanguage(CLuaH::luaScript &ls)
{
	name = CLuaH::getGlobalVarAsString(ls, "langName");
	entries["menuPlayTitle"] = CLuaH::getGlobalVarAsString(ls, "menuPlayTitle");
	entries["menuOptionsTitle"] = CLuaH::getGlobalVarAsString(ls, "menuOptionsTitle");
}

CLanguageManager::CLanguage::CLanguage()
{
	loaded = false;
}

std::string CLanguageManager::getText(const std::string &lang, const std::string &entry)
{
	return langs[lang].entries[entry];
}

CLanguageManager::CLanguageManager()
{
	auto &lua = CLuaH::Lua();
	const std::string langPath = "data/languages";

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

			langs[name] = CLanguage(langscript.second);
		}
	}
}

CLanguageManager &CLanguageManager::langMGR()
{
	static CLanguageManager mgr;
	return mgr;
}


