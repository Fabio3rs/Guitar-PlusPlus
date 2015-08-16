// Guitar++.cpp : Defines the entry point for the console application.
//

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

int main(int argc, char* argv[])
{
	CLog::log(); // Start logging before everything, to avoid non-logged crashes
	GPPGame::GuitarPP();

	// Configuration file
	auto &script = CLuaH::Lua().newScript(".", "Config.lua");

	if (script.luaState){
		CLuaH::Lua().runScript(script);
	}

	// Run all scripts in quere
	CLuaH::Lua().runScripts();

	// Window
	GPPGame::GuitarPP().createWindow();

	GPPGame::GuitarPP().loadBasicSprites();

	auto &mainMenu = GPPGame::GuitarPP().newMenu();
	int startOP, configOP, extrasOp, ajudaOp, quitOp;

	double proportion = GPPGame::GuitarPP().getWindowProportion();
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

		startOP = mainMenu.addOpt(opt);
	}

	{
		CMenu::menuOpt opt;

		opt.text = "Opções";
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

		quitOp = mainMenu.addOpt(opt);
	}

	GPPGame::GuitarPP().setMainMenu(mainMenu);
	CLuaH::Lua().runEvent("mainMenuSetted");

	GPPGame::GuitarPP().setVSyncMode(1);

	while (CEngine::engine().windowOpened()){
		GPPGame::GuitarPP().clearScreen();

		mainMenu.update();




		mainMenu.render();

		GPPGame::GuitarPP().renderFrame();
	}

	CLuaH::Lua().runEvent("atExit");
	return 0;
}

