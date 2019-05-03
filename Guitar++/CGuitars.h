#pragma once
#ifndef _CGUITARPP_CGUITARS_H_
#define _CGUITARPP_CGUITARS_H_

#include <map>
#include <string>
#include <fstream>
#include "CLuaH.hpp"
#include "GPPOBJ.h"

class CGamePlay;

class CGuitars
{
	static int addGuitarScriptEvent, loadGuitarScriptEvent, unloadingGuitarScriptEvent;

public:
	class CGuitar
	{
		friend CGuitars;
		friend CGamePlay;
		std::string path, internalName;
		std::string gameplayBarName;
		int textureID;
		unsigned int fretboardText;
		GPPOBJ gameplayBar;
		bool loaded;
		CLuaH::luaScript luaF;

	public:
		inline const std::string &getPath() noexcept
		{
			return path;
		}

		inline const std::string &getInternalName() noexcept
		{
			return internalName;
		}

		std::string name;

		void load();
		void unload();

		CGuitar() noexcept;
	};

	static CGuitars &inst();

	CGuitar &getGuitar(const std::string &internalName);
	CGuitar *getGuitarIfExists(const std::string &internalName);
	CGuitar &addGuitar(const std::string &path, const std::string &internalName);

	void loadAllGuitars();

	void unload();

protected:
	static int registerFunctions(CLuaH::luaState_t &L);
	static int registerGlobals(CLuaH::luaState_t &L);

	static int changeGuitarData(lua_State *L);

private:
	std::map <std::string, CGuitar> guitars;

	CGuitars();
};

#endif
