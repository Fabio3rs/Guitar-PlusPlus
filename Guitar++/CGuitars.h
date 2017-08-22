#pragma once
#ifndef _CGUITARPP_CGUITARS_H_
#define _CGUITARPP_CGUITARS_H_

#include "GPPOBJ.h"
#include <string>
#include <map>
#include <fstream>
#include "CLuaH.hpp"

class CGamePlay;

class CGuitars
{

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
		inline const std::string &getPath()
		{
			return path;
		}

		inline const std::string &getInternalName()
		{
			return internalName;
		}

		std::string name;

		void load();
		void unload();

		CGuitar();
	};

	static CGuitars &inst();

	CGuitar &getGuitar(const std::string &internalName);
	CGuitar *getGuitarIfExists(const std::string &internalName);
	CGuitar &addGuitar(const std::string &path, const std::string &internalName);

	void loadAllGuitars();

	void unload();

protected:
	static int registerFunctions(CLuaH::luaState &L);
	static int registerGlobals(CLuaH::luaState &L);

	static int changeGuitarData(lua_State *L);

private:
	std::map <std::string, CGuitar> guitars;

	CGuitars();
};

#endif
