#include "CCampaing.h"
#include "CLog.h"
#include "GPPGame.h"
#include "CFonts.h"
#include <dirent.h>

CCampaing::CCampaingData::CCampaingData()
{
	money = 0.0;
	reputationPoints = 0;
	mode = "default";
}

bool CCampaing::loadCampaingF(const std::string &filepath)
{
	try {
		std::fstream svfstream(filepath, std::ios::in | std::ios::binary);

		if (!svfstream.is_open())
		{
			return false;
		}

		cereal::BinaryInputArchive iarchive(svfstream);

		iarchive(campaingNow);

		campaingLoaded = true;
		loadedCampaingFilepath = filepath;

		std::string scriptsPath = campaingScriptsDirectory + "/" + campaingNow.mode;

		CLuaH::Lua().loadFilesDequeStorage(scriptsPath, campaingScripts);
		CLuaH::Lua().runScriptsFromDequeStorage(campaingScripts);
	}
	catch (const std::exception &e)
	{
		CLog::log() << e.what();
		campaingLoaded = false;
		return false;
	}
	catch (...)
	{
		CLog::log() << "Fail to load campaing data " + filepath;
		campaingLoaded = false;
		return false;
	}

	return true;
}

bool CCampaing::saveCampaingF()
{
	try {
		std::fstream svfstream(loadedCampaingFilepath, std::ios::out | std::ios::trunc | std::ios::binary);

		if (!svfstream.is_open())
		{
			return false;
		}

		cereal::BinaryOutputArchive oarchive(svfstream); // Create an output archive

		oarchive(campaingNow);
	}
	catch (const std::exception &e)
	{
		CLog::log() << e.what();
		return false;
	}
	catch (...)
	{
		CLog::log() << "Fail to save campaing data " + loadedCampaingFilepath;
		return false;
	}
	return true;
}

int CCampaing::getCampaingList(lua_State *L)
{
	CLuaFunctions::LuaParams p(L);

	p << "TODO";

	return p.rtn();
}

int CCampaing::registerLuaFunctions(lua_State *L)
{
	lua_register(L, "getCampaingList", getCampaingList);

	return 0;
}

CCampaing &CCampaing::campaingMGR()
{
	static CCampaing campaing;
	return campaing;
}

int CCampaing::newCampaing()
{
	campaingScripts.clear();

	campaingNow = CCampaingData();

	std::string scriptsPath = campaingScriptsDirectory + "/" + campaingNow.mode;
	CLuaH::Lua().loadFilesDequeStorage(scriptsPath, campaingScripts);

	saveCampaingF();


	return 0;
}

int CCampaing::continueCampaing(const std::string &path)
{
	campaingScripts.clear();




	return 0;
}

int CCampaing::campaingMenu()
{



	return 0;
}

int CCampaing::campaingMainMenu(CMenu &menu)
{
	auto &game = GPPGame::GuitarPP();

	menu.openCallback = openCampaingMenuCallback;

	{
		CMenu::menuOpt opt;

		opt.text = "Nova campanha";
		opt.y = 0.4;
		opt.size = 0.075;
		opt.x = CFonts::fonts().getCenterPos(opt.text, opt.size, -0.5);
		opt.group = 1;
		opt.status = 0;
		opt.type = CMenu::menusOPT::textbtn;

		menuNovaCampanhaID = menu.addOpt(opt);
	}

	{
		CMenu::menuOpt opt;

		opt.text = "Continuar campanha";
		opt.y = 0.3;
		opt.size = 0.075;
		opt.x = CFonts::fonts().getCenterPos(opt.text, opt.size, -0.5);
		opt.group = 1;
		opt.status = 0;
		opt.type = CMenu::menusOPT::textbtn;

		opt.menusXRef.push_back(game.addGameCallbacks("continueCampaingFunctionMGR", GPPGame::continueCampaing));
		opt.updateCppCallback = cotinueCampaingOptCallback;

		menuContinuarCampanhaID = menu.addOpt(opt);
	}

	{
		CMenu::menuOpt opt;

		opt.text = "Voltar";
		opt.y = -0.2;
		opt.size = 0.075;
		opt.x = CFonts::fonts().getCenterPos(opt.text, opt.size, -0.5);
		opt.group = 1;
		opt.status = 0;
		opt.type = CMenu::menusOPT::textbtn;
		opt.goback = true;

		menu.addOpt(opt);
	}

	return 0;
}

void CCampaing::loadCampaingModes()
{
	auto &lua = CLuaH::Lua();
	const std::string campaingModesPath = "data/campaings";

	DIR *direntd = opendir(("./" + campaingModesPath).c_str());
	dirent *rrd = nullptr;

	if (direntd)
	{
		rrd = readdir(direntd);
		while ((rrd = readdir(direntd)) != nullptr)
		{
			std::string dname = rrd->d_name;
			if ((rrd->d_type & DT_DIR) != 0 && dname != "." && dname != "..")
			{
				std::string rpath = campaingModesPath + "/" + dname;
				std::string fullpath = rpath + "/info.lua";
				CLog::log() << ("Loading <<" + fullpath + ">>");

				{
					if (!std::fstream(fullpath, std::ios::in | std::ios::binary).is_open())
					{
						CLog::log() << ("<<" + fullpath + ">> does not exist");

						continue;
					}
				}

				auto infoScript = lua.newScript(rpath, "info.lua");

				if (infoScript.luaState != nullptr)
				{
					lua.runScript(infoScript);

					auto &mode = campaingModes[dname];

					mode.name = CLuaH::getGlobalVarAsString(infoScript, "name");
					mode.description = CLuaH::getGlobalVarAsString(infoScript, "description");
					mode.author = CLuaH::getGlobalVarAsString(infoScript, "author");
				}
			}
		}
		closedir(direntd);
	}
}

std::deque<std::string> CCampaing::listCampaingSaves()
{
	return GPPGame::getDirectory("./data/saves/campaings", false, true);
}

int CCampaing::openCampaingMenuCallback(CMenu & menu)
{
	campaingMGR().numCampaingSaves = listCampaingSaves().size();

	return 0;
}

int CCampaing::cotinueCampaingOptCallback(CMenu::menuOpt &opt)
{
	if (campaingMGR().numCampaingSaves <= 0)
	{
		opt.enableEnter = false;
	}
	else
	{
		opt.enableEnter = true;
	}
	return 0;
}

CCampaing::CCampaing() : campaingScriptsDirectory("./data/campaings")
{
	numCampaingSaves = 0;
	menuNovaCampanhaID = menuContinuarCampanhaID = 0;
	campaingLoaded = false;
	CLuaFunctions::LuaF().registerLuaFuncsAPI(registerLuaFunctions);
}
