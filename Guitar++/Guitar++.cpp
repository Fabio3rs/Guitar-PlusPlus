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

int main(int argc, char* argv[])
{
	CLog::log(); // Start logging before everything, to avoid non-logged crashes
	auto &game = GPPGame::GuitarPP();

	game.parseParameters(argc, argv);


	auto &lua = CLuaH::Lua();


	// Configuration file
	auto &script = lua.newScript(".", "Config.lua");

	if (script.luaState){
		lua.runScript(script);
	}

	// Run all scripts in quere
	lua.runScripts();

	// Window
	game.createWindow();


	game.loadAllThemes();
	game.gThemes["gppdefaulttheme"].apply();




	game.loadBasicSprites();

	auto &mainMenu = game.newMenu();
	int startOP, configOP, extrasOp, ajudaOp, quitOp;

	double proportion = game.getWindowProportion();
	//difBtIMGAndProp = 1.7777777777777777777777 - proportion;

	{
		CMenu::menuOpt opt;

		opt.text = "Jogar";
		opt.y = 0.4;
		opt.x = -proportion + 0.15;
		opt.size = 0.075;
		opt.group = 1;
		opt.status = 0;
		opt.type = CMenu::menusOPT::textbtn;

		std::string testecallback = game.addGameCallbacks("gamePlaySelectedInMenu", GPPGame::startModule);
		opt.menusXRef.push_back(testecallback);

		startOP = mainMenu.addOpt(opt);
	}

	{
		CMenu::menuOpt opt;
		//  
		opt.text = "Op\xC3\xA7\xC3\xB5" "es";
		opt.y = 0.3;
		opt.x = -proportion + 0.15;
		opt.size = 0.075;
		opt.group = 1;
		opt.status = 0;
		opt.type = CMenu::menusOPT::textbtn;

		configOP = mainMenu.addOpt(opt);
	}

	{
		CMenu::menuOpt opt;

		opt.text = "Extras";
		opt.y = 0.2;
		opt.x = -proportion + 0.15;
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
		opt.x = -proportion + 0.15;
		opt.size = 0.075;
		opt.group = 1;
		opt.status = 0;
		opt.type = CMenu::menusOPT::textbtn;

		ajudaOp = mainMenu.addOpt(opt);
	}

	{
		CMenu::menuOpt opt;

		opt.text = "Quit";
		opt.y = 0.0;
		opt.x = -proportion + 0.15;
		opt.size = 0.075;
		opt.group = 1;
		opt.status = 0;
		opt.type = CMenu::menusOPT::textbtn;
		opt.goback = true;

		quitOp = mainMenu.addOpt(opt);
	}

	game.setMainMenu(mainMenu);
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

	game.openMenus(game.getMainMenu());


	lua.runEvent("atExit");
	lua.unloadAll();
	return 0;
}

