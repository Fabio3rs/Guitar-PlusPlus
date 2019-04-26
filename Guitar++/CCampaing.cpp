#include "CCampaing.h"
#include "CLog.h"
#include "GPPGame.h"
#include "CFonts.h"
#include <dirent.h>

CCampaing::CCampaingData::CCampaingData()
{
	money = 0.0;
	playedTime = 0.0;
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
			auto lscript = CLuaH::Lua().newScriptRBuffer(campaingNow.scripts[i].byteCode, campaingNow.scripts[i].name);

			if (lscript.luaState == nullptr)
			{
				CLog::log() << std::string("Lua script load failed") + std::to_string(campaingNow.scripts[i].byteCode.size());
				continue;
			}

			campaingScripts.push_back(std::move(lscript));
			auto &tdata = campaingNow.scripts[i].scriptVars.getTableData();

			for (auto &t : tdata)
			{
				t.second.pushToLuaStack(campaingScripts[i].luaState.get());
				lua_setglobal(campaingScripts[i].luaState.get(), t.first.c_str());
			}

			CLuaFunctions::LuaF().registerFunctions(campaingScripts[i].luaState);
			CLuaFunctions::LuaF().registerGlobals(campaingScripts[i].luaState);
		}

		CLuaH::Lua().runScriptsFromStorage(campaingScripts);
		CLuaH::Lua().runEventFromContainer("campaingLoad", campaingScripts);
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
			CLuaH::loadGlobalTable(campaingScripts[i].luaState.get(), campaingNow.scripts[i].scriptVars);
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

int CCampaing::registerLuaFunctions(CLuaH::luaState &Lstate)
{
	lua_State *L = Lstate.get();

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

	CLuaH::Lua().runScriptsFromStorage(campaingScripts);
	CLuaH::Lua().runEventFromContainer("campaingInit", campaingScripts);

	saveCampaingF();

	return 0;
}

int CCampaing::continueCampaing(const std::string &path)
{
	//campaingScripts.clear();




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

		opt.menusXRef.push_back(mainMenu);
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

				auto infoScript = lua.newScript(rpath, std::string("info.lua"));

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
	/*auto &mgr = campaingMGR();
	auto &engine = CEngine::engine();
	auto &GuitarPP = GPPGame::GuitarPP();
	*/
	//bool windowOpened = true;


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

	std::vector <CMenu*> menusStack;

	bool windowOpened = true;

	auto preFun = [&]()
	{
		CLuaH::Lua().runEventFromContainer("campaingMenuPreFun", mgr.campaingScripts);
		return 0;
	};

	auto midFun = [&]()
	{
		CLuaH::Lua().runEventFromContainer("campaingMenuMidFun", mgr.campaingScripts);
		return 0;
	};

	auto posFun = [&]()
	{
		CLuaH::Lua().runEventFromContainer("campaingMenuPosFun", mgr.campaingScripts);
		return 0;
	};

	while ((windowOpened = engine.windowOpened()) && mgr.continueInDrawScreen)
	{
		GuitarPP.clearScreen();

		if (engine.getKey(GLFW_KEY_ESCAPE))
		{
			break;
		}

		CLuaH::Lua().runEventFromContainer("campaingDrawScreenUpdate", mgr.campaingScripts);

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

	return 0;
}

int CCampaing::openCampaingMenuCallback(CMenu &menu)
{
	campaingMGR().numCampaingSaves = listCampaingSaves().size();

	return 0;
}

int CCampaing::cotinueCampaingOptCallback(CMenu &menu, CMenu::menuOpt &opt)
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

void CCampaing::campaingPlayLoop(const std::string &n)
{
	auto &game = GPPGame::GuitarPP();
	{
		game.marathonSongsList = campaingMGR().playSongsList;
	}

	GPPGame::startMarathonModule(n);
}

CCampaing::CCampaing() : campaingScriptsDirectory("./data/campaings")
{
	auto &contOptions = GPPGame::GuitarPP().newNamedMenu("continueCampaingOptions");

	contOptions.openCallback = [](CMenu &m)
	{
		m.resetData();
		m.resetBtns();

		CLuaH::Lua().runEventWithParamsFromContainer("campaingMainMenuOpen", { CLuaH::customParam(&m) }, campaingMGR().campaingScripts);
		return 0;
	};

	{
		{
			CMenu::menuOpt opt;

			opt.text = "Modo campanha";
			opt.y = 0.8;
			opt.size = 0.1;
			opt.x = CFonts::fonts().getCenterPos(opt.text, opt.size, 0.0);
			//opt.group = 0;
			opt.status = 0;
			opt.type = CMenu::menusOPT::static_text;

			contOptions.addOpt(opt);
		}
		
		{
			CMenu::menuOpt opt;

			opt.text = "Jogar";
			opt.y = 0.4;
			opt.size = 0.075;
			opt.x = CFonts::fonts().getCenterPos(opt.text, opt.size, -0.5);
			opt.group = 1;
			opt.status = 0;
			opt.type = CMenu::menusOPT::textbtn;

			std::string testecallback = GPPGame::GuitarPP().addGameCallbacks("campaingGameplayLoop", campaingPlayLoop);
			opt.menusXRef.push_back(testecallback);

			contOptions.addOpt(opt);
		}

		double infoPos = 0.0;

		{
			CMenu::menuOpt opt;

			opt.text = "Informa\xc3\xa7ões:";
			opt.y = 0.3;
			opt.size = 0.075;
			opt.x = infoPos = CFonts::fonts().getCenterPos(opt.text, opt.size, -0.3);
			//opt.group = 0;
			opt.status = 0;
			opt.type = CMenu::menusOPT::static_text;

			contOptions.addOpt(opt);
		}

		{
			CMenu::menuOpt opt;

			opt.text = "* Show em um lugar público xyz";
			opt.y = 0.2;
			opt.size = 0.075;
			opt.x = infoPos;
			//opt.group = 0;
			opt.status = 0;
			opt.type = CMenu::menusOPT::static_text;

			contOptions.addOpt(opt);
		}

		{
			CMenu::menuOpt opt;

			opt.text = "* Várias músicas";
			opt.y = 0.11;
			opt.size = 0.075;
			opt.x = infoPos;
			//opt.group = 0;
			opt.status = 0;
			opt.type = CMenu::menusOPT::static_text;

			contOptions.addOpt(opt);
		}

		{
			CMenu::menuOpt opt;

			opt.text = "* Possibilidade de música aleatoria";
			opt.y = 0.02;
			opt.size = 0.075;
			opt.x = infoPos;
			//opt.group = 0;
			opt.status = 0;
			opt.type = CMenu::menusOPT::static_text;

			contOptions.addOpt(opt);
		}

		{
			CMenu::menuOpt opt;

			opt.text = "* Reputação máxima prevista 10000";
			opt.y = -0.07;
			opt.size = 0.075;
			opt.x = infoPos;
			//opt.group = 0;
			opt.status = 0;
			opt.type = CMenu::menusOPT::static_text;

			contOptions.addOpt(opt);
		}

		{
			CMenu::menuOpt opt;

			opt.text = "* Musicas previstas:";
			opt.y = -0.16;
			opt.size = 0.075;
			opt.x = infoPos;
			//opt.group = 0;
			opt.status = 0;
			opt.type = CMenu::menusOPT::static_text;

			contOptions.addOpt(opt);
		}

		{
			auto songs = GPPGame::getDirectory("./data/songs", false, true);

			std::random_shuffle(songs.begin(), songs.end());

			const unsigned int songsSize = 2 + rand() % 2;
			const unsigned int size = (songs.size() > songsSize)? songsSize : songs.size();

			if (size < songs.size())
			{
				songs.resize(size);
			}

			for (int i = 0; i < size; i++)
			{
				{
					CMenu::menuOpt opt;

					opt.text = songs[i];
					opt.y = -0.25 - i * 0.08;
					opt.size = 0.075;
					opt.x = infoPos + 0.075 * 2.0;
					//opt.group = 0;
					opt.status = 0;
					opt.type = CMenu::menusOPT::static_text;

					contOptions.addOpt(opt);
				}
			}

			playSongsList = std::move(songs);
		}

		{
			CMenu::menuOpt opt;

			opt.text = "Voltar";
			opt.y = -0.8;
			opt.size = 0.075;
			opt.x = CFonts::fonts().getCenterPos(opt.text, opt.size, -0.5);
			opt.group = 1;
			opt.status = 0;
			opt.type = CMenu::menusOPT::textbtn;
			opt.goback = true;

			contOptions.addOpt(opt);
		}
	}


	mainMenu = contOptions.getName();

	numCampaingSaves = 0;
	menuNovaCampanhaID = menuContinuarCampanhaID = 0;
	campaingFunctionGotoID = 0;
	keepMenuStack = false;
	campaingLoaded = false;
	continueInDrawScreen = false;
	loadedCampaingFilepath = "./data/saves/campaings/campaingZoeira/save";
	CLuaFunctions::LuaF().registerLuaFuncsAPI(registerLuaFunctions);
}
