#include "CLanguageManager.h"
#include <iostream>

CLanguageManager::CLanguage::CLanguage(CLuaH::luaScript &ls)
{
	luaf = &ls;
	name = CLuaH::getGlobalVarAsString(ls, "langName");
	entries["menuPlayTitle"] = CLuaH::getGlobalVarAsString(ls, "menuPlayTitle");
	entries["menuOptionsTitle"] = CLuaH::getGlobalVarAsString(ls, "menuOptionsTitle");
	entries["menuExtrasTitle"] = CLuaH::getGlobalVarAsString(ls, "menuExtrasTitle");

	loaded = true;
}

CLanguageManager::CLanguage::CLanguage()
{
	loaded = false;
	luaf = nullptr;
}

std::string CLanguageManager::getLanguageName(const std::string &lang)
{
	return langs[lang].name;
}

std::string CLanguageManager::getTextDirectFromFile(const std::string &lang, const std::string &entry)
{
	if (langs[lang].luaf == nullptr)
		return "Lang file " + lang + " not loaded";

	return CLuaH::getGlobalVarAsString(*langs[lang].luaf, entry);
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


