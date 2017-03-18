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

		campaingScripts.clear();

		for (int i = 0, size = campaingNow.scripts.size(); i < size; i++)
		{
			auto &lscript = CLuaH::Lua().newScriptRBuffer(campaingNow.scripts[i].byteCode, campaingNow.scripts[i].name);

			if (lscript.luaState == nullptr)
			{
				CLog::log() << std::string("Lua script load failed") + std::to_string(campaingNow.scripts[i].byteCode.size());
				continue;
			}

			campaingScripts.push_back(std::move(lscript));
			auto &tdata = campaingNow.scripts[i].scriptVars.getTableData();

			for (auto &t : tdata)
			{
				t.second.pushToLuaStack(campaingScripts[i].luaState);
				lua_setglobal(campaingScripts[i].luaState, t.first.c_str());
			}

			CLuaFunctions::LuaF().registerFunctions(campaingScripts[i].luaState);
			CLuaFunctions::LuaF().registerGlobals(campaingScripts[i].luaState);
		}

		CLuaH::Lua().runScriptsFromDequeStorage(campaingScripts);
		CLuaH::Lua().runEventFromDeque("campaingLoad", campaingScripts);
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

		for (int i = 0, size = campaingNow.scripts.size(); i < size; i++)
		{
			campaingNow.scripts[i].scriptVars.clear();
			CLuaH::loadGlobalTable(campaingScripts[i].luaState, campaingNow.scripts[i].scriptVars);
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
	lua_register(L, "getBandName", getBandName);
	lua_register(L, "getCampaingMode", getCampaingMode);
	lua_register(L, "getBandMoney", getBandMoney);
	lua_register(L, "getBandReputationPoints", getBandReputationPoints);
	lua_register(L, "isCampaingLoaded", isCampaingLoaded);
	lua_register(L, "getLoadedCampaingPath", getLoadedCampaingPath);
	lua_register(L, "keepCampaingMenuStack", keepCampaingMenuStack);
	lua_register(L, "exitCampaingScreen", exitCampaingScreen);

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

	campaingNow.scripts.clear();

	for (int i = 0, size = campaingScripts.size(); i < size; i++)
	{
		luaScriptSave nluascriptsave;

		nluascriptsave.byteCode = campaingScripts[i].dumpBytecode();
		nluascriptsave.name = campaingScripts[i].fileName;
		nluascriptsave.scriptVars.clear();

		if (nluascriptsave.byteCode.size() == 0)
			CLog::log() << std::string("Script bytecode ") + std::to_string(nluascriptsave.byteCode.size()) + " " + campaingScripts[i].fileName;

		campaingNow.scripts.push_back(nluascriptsave);
	}

	CLuaH::Lua().runScriptsFromDequeStorage(campaingScripts);
	CLuaH::Lua().runEventFromDeque("campaingInit", campaingScripts);

	saveCampaingF();

	return 0;
}

int CCampaing::continueCampaing(const std::string &path)
{
	campaingScripts.clear();




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

int CCampaing::getBandName(lua_State *L)
{
	CLuaFunctions::LuaParams p(L);

	p << campaingMGR().campaingNow.bandName;

	return p.rtn();
}

int CCampaing::getCampaingMode(lua_State *L)
{
	CLuaFunctions::LuaParams p(L);

	p << campaingMGR().campaingNow.mode;

	return p.rtn();
}

int CCampaing::getBandMoney(lua_State *L)
{
	CLuaFunctions::LuaParams p(L);

	p << campaingMGR().campaingNow.money;

	return p.rtn();
}

int CCampaing::getBandReputationPoints(lua_State *L)
{
	CLuaFunctions::LuaParams p(L);

	p << campaingMGR().campaingNow.reputationPoints;

	return p.rtn();
}

int CCampaing::isCampaingLoaded(lua_State *L)
{
	CLuaFunctions::LuaParams p(L);

	p << campaingMGR().campaingLoaded;

	return p.rtn();
}

int CCampaing::getLoadedCampaingPath(lua_State *L)
{
	CLuaFunctions::LuaParams p(L);

	p << campaingMGR().loadedCampaingFilepath;

	return p.rtn();
}

int CCampaing::keepCampaingMenuStack(lua_State * L)
{
	CLuaFunctions::LuaParams p(L);

	p >> campaingMGR().keepMenuStack;

	return p.rtn();
}

int CCampaing::exitCampaingScreen(lua_State * L)
{
	CLuaFunctions::LuaParams p(L);

	campaingMGR().continueInDrawScreen = false;

	return p.rtn();
}

int CCampaing::campaingLoop()
{
	auto &mgr = campaingMGR();
	auto &engine = CEngine::engine();
	auto &GuitarPP = GPPGame::GuitarPP();

	bool windowOpened = true;

	

	return 0;
}

int CCampaing::campaingDrawScreen()
{
	auto &mgr = campaingMGR();
	auto &engine = CEngine::engine();
	auto &GuitarPP = GPPGame::GuitarPP();

	callOnDctor<void(void)> exitguard([]()
	{
		auto &mgr = campaingMGR();

		mgr.campaingFunctionGotoID = 0;
		mgr.continueInDrawScreen = 0;
	});

	std::deque <CMenu*> menusStack;

	bool windowOpened = true;

	auto preFun = [&]()
	{
		CLuaH::Lua().runEventFromDeque("campaingMenuPreFun", mgr.campaingScripts);
		return 0;
	};

	auto midFun = [&]()
	{
		CLuaH::Lua().runEventFromDeque("campaingMenuMidFun", mgr.campaingScripts);
		return 0;
	};

	auto posFun = [&]()
	{
		CLuaH::Lua().runEventFromDeque("campaingMenuPosFun", mgr.campaingScripts);
		return 0;
	};

	while ((windowOpened = engine.windowOpened()) && mgr.continueInDrawScreen)
	{
		GuitarPP.clearScreen();

		if (engine.getKey(GLFW_KEY_ESCAPE))
		{
			break;
		}

		CLuaH::Lua().runEventFromDeque("campaingDrawScreenUpdate", mgr.campaingScripts);

		GuitarPP.renderFrame();

		switch (mgr.campaingFunctionGotoID)
		{
		case 0:
			break;

		case 1:
			menusStack = GuitarPP.openMenus(&(mgr.campaingMenu), preFun, midFun, posFun, false, std::move(menusStack));

			if (!mgr.keepMenuStack)
			{
				menusStack.clear();
			}
			break;

		default:
			break;
		}
	}
}

int CCampaing::openCampaingMenuCallback(CMenu &menu)
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
	campaingFunctionGotoID = 0;
	keepMenuStack = false;
	campaingLoaded = false;
	continueInDrawScreen = false;
	loadedCampaingFilepath = "./data/saves/campaings/campaingZoeira/save";
	CLuaFunctions::LuaF().registerLuaFuncsAPI(registerLuaFunctions);
}
