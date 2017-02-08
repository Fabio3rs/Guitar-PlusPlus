#include "CGuitars.h"
#include <dirent.h>
#include "CLuaFunctions.hpp"
#include "CLog.h"

void CGuitars::unload()
{
	guitars.clear();
}

CGuitars::CGuitar &CGuitars::addGuitar(const std::string &path, const std::string &internalName)
{
	std::string fullPath;
	CGuitar &newGuitar = guitars[internalName];

	newGuitar.path = path;
	newGuitar.internalName = internalName;
	fullPath = path + "/" + internalName;

	newGuitar.luaF = CLuaH::Lua().newScript(fullPath, "GuitarScript.lua");

	if (newGuitar.luaF.luaState != nullptr)
	{
		CLuaH::multiCallBackParams_t g = { path, internalName };

		CLuaH::Lua().runScript(newGuitar.luaF);

		CLuaH::Lua().runInternalEventWithParams(newGuitar.luaF, "addGuitar", g);
	}
	else
	{
		CLog::log() << "CGuitars::addGuitar <<script.luaState = nullptr>>";
	}

	return newGuitar;
}

CGuitars::CGuitar &CGuitars::getGuitar(const std::string &internalName)
{
	return guitars[internalName];
}

CGuitars::CGuitar *CGuitars::getGuitarIfExists(const std::string & internalName)
{
	auto it = guitars.find(internalName);

	if (it != guitars.end())
		return &((*it).second);

	return nullptr;
}

void CGuitars::CGuitar::load()
{
	if (this == nullptr)
		return;
	if (!loaded)
	{
		loaded = true;

		CLuaH::multiCallBackParams_t g = { path, internalName };

		gameplayBar.load((path + "/" + internalName + "/" + gameplayBarName).c_str());

		CLuaH::Lua().runInternalEventWithParams(luaF, "loadGuitar", g);
	}
}

void CGuitars::CGuitar::unload()
{
	loaded = false;

	// TODO unloader


}

CGuitars::CGuitar::CGuitar()
{
	loaded = false;
	textureID = 0;
	fretboardText = 0;
}

void CGuitars::loadAllGuitars()
{
	auto extension_from_filename = [](const std::string &fname)
	{
		size_t s;
		return std::string(((s = fname.find_first_of('.')) != fname.npos) ? (&fname.c_str()[++s]) : (""));
	};

	auto file_exists = [](const std::string &fileName) {
		return std::fstream(fileName).is_open();
	};

	DIR *direntd = opendir("./data/guitars");
	dirent *rrd = nullptr;

	if (direntd)
	{
		rrd = readdir(direntd);
		while ((rrd = readdir(direntd)) != nullptr)
		{
			std::string name = rrd->d_name;

			if (name != ".." && name != ".")
			{
				if ((rrd->d_type & DT_DIR) != 0 && file_exists(std::string("./data/guitars/") + name + "/GuitarScript.lua"))
				{
					addGuitar("./data/guitars", rrd->d_name);
				}
			}
		}
		closedir(direntd);
	}
}

int CGuitars::changeGuitarData(lua_State *L)
{
	const static std::map<std::string, int> mp = { { "name", 0 }, { "gameplayBarName", 1 }, { "textureID", 2 }, { "fretboardText", 3 } };

	CLuaFunctions::LuaParams p(L);

	if (p.getNumParams() >= 3)
	{
		std::string targetGuitar;

		p >> targetGuitar;

		auto it = inst().guitars.find(targetGuitar);

		if (it != inst().guitars.end())
		{
			std::string dataName;
			p >> dataName;

			auto it2 = mp.find(dataName);

			if (it2 != mp.end())
			{
				switch ((*it2).second)
				{
				case 0:
					p >> (*it).second.name;
					break;
					
				case 1:
					p >> (*it).second.gameplayBarName;
					break;

				case 2:
					p >> (*it).second.textureID;
					break;

				case 3:
					p >> (*it).second.fretboardText;
					break;

				default:
					break;
				}

				p << true;
			}
			else
			{
				p << false;
				p << "Propertie not found";
			}
		}
		else
		{
			p << false;
			p << "Guitar not found";
		}
	}
	else
	{
		p << false;
		p << "Insuficient parameters";
	}

	return p.rtn();
}

int CGuitars::registerFunctions(lua_State *L)
{
	lua_register(L, "changeGuitarData", changeGuitarData);
	return 0;
}

int CGuitars::registerGlobals(lua_State *L)
{

	return 0;
}

CGuitars &CGuitars::inst()
{
	static CGuitars guitars;
	return guitars;
}

CGuitars::CGuitars()
{
	CLuaFunctions::LuaF().registerLuaFuncsAPI(registerFunctions);
	CLuaFunctions::LuaF().registerLuaFuncsAPI(registerGlobals);

}
