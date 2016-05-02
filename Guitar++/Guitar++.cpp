// Guitar++.cpp : Defines the entry point for the console application.
//
// brmodstudio.forumeiros.com

#include <iostream>
#include <exception>
#include <sstream>
#include <fstream>
#include <string>
#include "GPPGame.h"
#include "CLuaH.hpp"
#include "CEngine.h"
#include "CLog.h"
#include "CMenu.h"
#include <Windows.h>
#include <Psapi.h>
#include "CFonts.h"
#include "CPlayer.h"
#include "CSaveSystem.h"
#include "CLanguageManager.h"

int main(int argc, char* argv[])
{
	try{
		CEngine::engine(GPPGame::logError);

		try{
			CLog::log(); // Start logging before everything, to avoid non-logged crashes
		}
		catch (const std::exception &e)
		{
			MessageBoxA(0, e.what(), "Error", 0);
			return 1;
		}


		auto &lngmgr = CLanguageManager::langMGR();
		//int variaveldopato = 0;

		//std::cout << "@_@: " << svtest.getVarContent<int>("variaveldopato") << std::endl;

		auto &game = GPPGame::GuitarPP();

		game.parseParameters(argc, argv);

		//svtest.addVariableAttData("variaveldopato", variaveldopato, true);
		//variaveldopato = 10;

		//svtest.saves();

		auto &lua = CLuaH::Lua();


		// Configuration file
		auto &script = lua.newScript(".", "Config.lua");

		if (script.luaState)
		{
			lua.runScript(script);
		}

		// Run all scripts in quere
		lua.runScripts();

		// Window
		game.createWindow();


		game.loadAllThemes();
		game.gThemes["gppdefaulttheme"].apply();

		auto &logotext = game.loadTexture("data/sprites", "logo.tga");

		double ltime = CEngine::engine().getTime();

		CEngine::engine().activate3DRender(false);

		CEngine::RenderDoubleStruct lgrdata;

		lgrdata.x1 = -0.5;
		lgrdata.x2 = -0.5 + 1.0;
		lgrdata.x3 = -0.5 + 1.0;
		lgrdata.x4 = -0.5;

		lgrdata.y1 = -0.5 + 1.0;
		lgrdata.y2 = -0.5 + 1.0;
		lgrdata.y3 = -0.5;
		lgrdata.y4 = -0.5;

		lgrdata.TextureX1 = 0.0;
		lgrdata.TextureX2 = 1.0;

		lgrdata.TextureY1 = 1.0;
		lgrdata.TextureY2 = 0.0;

		lgrdata.Text = logotext.getTextId();

		std::string str2nibblePresents = "2Nibble Studios presents...";

		while (CEngine::engine().windowOpened() && (CEngine::engine().getTime() - ltime) < 3.0)
		{
			if (CEngine::engine().getKey(GLFW_KEY_ESCAPE))
			{
				break;
			}

			GPPGame::GuitarPP().clearScreen();
			double t = (CEngine::engine().getTime() - ltime) * 3.14 / 3.0, t2;

			CEngine::engine().setColor(1.0, 1.0, 1.0, sin(t));
			CEngine::engine().Render2DQuad(lgrdata);

			CEngine::engine().setColor(1.0, 1.0, 1.0, sin(t + 0.1));
			CFonts::fonts().drawTextInScreen(str2nibblePresents, CFonts::fonts().getCenterPos(str2nibblePresents.size(), 0.1, 0.0), -0.6, 0.1);
			

			CEngine::engine().setColor(1.0, 1.0, 1.0, 1.0);
			GPPGame::GuitarPP().renderFrame();
		}

		if (!CEngine::engine().windowOpened())
		{
			return 0;
		}

		game.loadBasicSprites();

		auto &mainMenu = game.newMenu();
		auto &gmenu = GPPGame::GuitarPP().newNamedMenu("playOptions");
		auto &cmpopts = GPPGame::GuitarPP().newNamedMenu("mainCampaingOptions");
		auto &optionsmenu = GPPGame::GuitarPP().newNamedMenu("optionsmenu");
		int startOP, configOP, extrasOp, ajudaOp, quitOp, singlePlayOp;

		double proportion = game.getWindowProportion();
		//difBtIMGAndProp = 1.7777777777777777777777 - proportion;

		{
			CMenu::menuOpt opt;

			opt.text = lngmgr.getText(game.glanguage, "menuPlayTitle");
			opt.langEntryKey = "menuPlayTitle";
			opt.y = 0.4;
			opt.x = -proportion + 0.2;
			opt.size = 0.075;
			opt.group = 1;
			opt.status = 0;
			opt.type = CMenu::menusOPT::textbtn;

			//std::string testecallback = game.addGameCallbacks("gamePlaySelectedInMenu", GPPGame::startModule);
			//opt.menusXRef.push_back(game.addGameCallbacks("gamePlaySelectedInMenu", GPPGame::startModule));
			opt.menusXRef.push_back(gmenu.getName());

			startOP = mainMenu.addOpt(opt);
		}

		{
			CMenu::menuOpt opt;
			//  
			//opt.text = "Op\xC3\xA7\xC3\xB5" "es";
			opt.text = lngmgr.getText(game.glanguage, "menuOptionsTitle");
			opt.langEntryKey = "menuOptionsTitle";
			opt.y = 0.3;
			opt.x = -proportion + 0.2;
			opt.size = 0.075;
			opt.group = 1;
			opt.status = 0;
			opt.type = CMenu::menusOPT::textbtn;
			
			opt.menusXRef.push_back(optionsmenu.getName());


			configOP = mainMenu.addOpt(opt);
		}

		{
			CMenu::menuOpt opt;

			opt.text = "Extras";
			opt.y = 0.2;
			opt.x = -proportion + 0.2;
			opt.size = 0.075;
			opt.group = 1;
			opt.status = 0;
			opt.type = CMenu::menusOPT::textbtn;

			extrasOp = mainMenu.addOpt(opt);
		}

		{
			CMenu::menuOpt opt;

			opt.text = "Ajuda";
			opt.y = 0.1;
			opt.x = -proportion + 0.2;
			opt.size = 0.075;
			opt.group = 1;
			opt.status = 0;
			opt.type = CMenu::menusOPT::textbtn;

			std::string testecallback = game.addGameCallbacks("gameHelpMenu", GPPGame::helpMenu);
			opt.menusXRef.push_back(testecallback);

			ajudaOp = mainMenu.addOpt(opt);
		}

		{
			CMenu::menuOpt opt;

			opt.text = "Sair";
			opt.y = 0.0;
			opt.x = -proportion + 0.2;
			opt.size = 0.075;
			opt.group = 1;
			opt.status = 0;
			opt.type = CMenu::menusOPT::textbtn;
			opt.goback = true;

			quitOp = mainMenu.addOpt(opt);
		}

		game.setMainMenu(mainMenu);

		{
			CMenu::menuOpt opt;

			opt.text = "Quick play";
			opt.y = 0.4;
			opt.size = 0.075;
			opt.x = CFonts::fonts().getCenterPos(opt.text.size(), opt.size, 0.0);
			opt.group = 1;
			opt.status = 0;
			opt.type = CMenu::menusOPT::textbtn;

			std::string testecallback = game.addGameCallbacks("gamePlaySelectedInMenu", GPPGame::startModule);
			opt.menusXRef.push_back(testecallback);

			singlePlayOp = gmenu.addOpt(opt);
		}

		{
			CMenu::menuOpt opt;

			opt.text = "Campaing";
			opt.y = 0.3;
			opt.size = 0.075;
			opt.x = CFonts::fonts().getCenterPos(opt.text.size(), opt.size, 0.0);
			opt.group = 1;
			opt.status = 0;
			opt.type = CMenu::menusOPT::textbtn;

			opt.menusXRef.push_back(cmpopts.getName());
			gmenu.addOpt(opt);
		}

		{
			CMenu::menuOpt opt;

			opt.text = "Voltar";
			opt.y = 0.0;
			opt.size = 0.075;
			opt.x = CFonts::fonts().getCenterPos(opt.text.size(), opt.size, 0.0);
			opt.group = 1;
			opt.status = 0;
			opt.type = CMenu::menusOPT::textbtn;
			opt.goback = true;

			gmenu.addOpt(opt);
		}


		//////////////////***********************************************************

		{
			CMenu::menuOpt opt;

			opt.text = "Nova campanha";
			opt.y = 0.4;
			opt.size = 0.075;
			opt.x = CFonts::fonts().getCenterPos(opt.text.size(), opt.size, -0.5);
			opt.group = 1;
			opt.status = 0;
			opt.type = CMenu::menusOPT::textbtn;

			cmpopts.addOpt(opt);
		}

		{
			CMenu::menuOpt opt;

			opt.text = "Continuar campanha";
			opt.y = 0.3;
			opt.size = 0.075;
			opt.x = CFonts::fonts().getCenterPos(opt.text.size(), opt.size, -0.5);
			opt.group = 1;
			opt.status = 0;
			opt.type = CMenu::menusOPT::textbtn;

			opt.menusXRef.push_back(game.addGameCallbacks("continueCampaingFunctionMGR", GPPGame::continueCampaing));

			cmpopts.addOpt(opt);
		}

		{
			CMenu::menuOpt opt;

			opt.text = "Voltar";
			opt.y = -0.2;
			opt.size = 0.075;
			opt.x = CFonts::fonts().getCenterPos(opt.text.size(), opt.size, -0.5);
			opt.group = 1;
			opt.status = 0;
			opt.type = CMenu::menusOPT::textbtn;
			opt.goback = true;

			cmpopts.addOpt(opt);
		}

		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


		{
			CMenu::menuOpt opt;

			opt.text = "Graficos";
			opt.y = 0.4;
			opt.size = 0.075;
			opt.x = CFonts::fonts().getCenterPos(opt.text.size(), opt.size, 0.0);
			opt.group = 1;
			opt.status = 0;
			opt.type = CMenu::menusOPT::textbtn;

			optionsmenu.addOpt(opt);
		}

		{
			CMenu::menuOpt opt;

			opt.text = "Audio";
			opt.y = 0.3;
			opt.size = 0.075;
			opt.x = CFonts::fonts().getCenterPos(opt.text.size(), opt.size, 0.0);
			opt.group = 1;
			opt.status = 0;
			opt.type = CMenu::menusOPT::textbtn;

			optionsmenu.addOpt(opt);
		}

		{
			CMenu::menuOpt opt;

			opt.text = "Gameplay";
			opt.y = 0.2;
			opt.size = 0.075;
			opt.x = CFonts::fonts().getCenterPos(opt.text.size(), opt.size, 0.0);
			opt.group = 1;
			opt.status = 0;
			opt.type = CMenu::menusOPT::textbtn;

			optionsmenu.addOpt(opt);
		}

		{
			CMenu::menuOpt opt;

			opt.text = "Voltar";
			opt.y = -0.4;
			opt.size = 0.075;
			opt.x = CFonts::fonts().getCenterPos(opt.text.size(), opt.size, 0.0);
			opt.group = 1;
			opt.status = 0;
			opt.type = CMenu::menusOPT::textbtn;
			opt.goback = true;

			optionsmenu.addOpt(opt);
		}


		lua.runEvent("mainMenuSetted");



		GPPGame::GuitarPP().setVSyncMode(1);

		// menu background texture
		auto &menu = game.loadTexture("data/sprites", "menu.tga");


		//***********************************************
		CEngine::RenderDoubleStruct RenderData;

		double prop = (double)menu.getImgWidth() / (double)menu.getImgHeight();

		RenderData.x1 = -prop;
		RenderData.x2 = prop;
		RenderData.x3 = prop;
		RenderData.x4 = -prop;

		RenderData.y1 = 1.0;
		RenderData.y2 = 1.0;
		RenderData.y3 = -1.0;
		RenderData.y4 = -1.0;

		RenderData.z1 = 0.0;
		RenderData.z2 = 0.0;
		RenderData.z3 = 0.0;
		RenderData.z4 = 0.0;

		RenderData.TextureX1 = 0.0;
		RenderData.TextureX2 = 1.0;
		RenderData.TextureY1 = 1.0;
		RenderData.TextureY2 = 0.0;

		RenderData.Text = menu.getTextId();
		//////////////////////////////////////////////


		//game.getMainMenu()->backgroundTexture = menu.getGTextureName();
		game.openMenus(game.getMainMenu());


		lua.runEvent("atExit");
		lua.unloadAll();
	} catch (const std::exception &e)
	{
		CLog::log() << e.what();
	}
	return 0;
}

