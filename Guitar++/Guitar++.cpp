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
#include <Windows.h>
#include <Psapi.h>
#include "CFonts.h"

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

	//GPPGame::GuitarPP().setVSyncMode(1);

	auto &menu = GPPGame::GuitarPP().loadTexture("data/sprites", "menu.tga");

	CEngine::RenderDoubleStruct RenderData;

	std::cout << menu.getImgHeight() << "   " << menu.getImgWidth() << std::endl;

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

	while (CEngine::engine().windowOpened()){
		GPPGame::GuitarPP().clearScreen();

		mainMenu.update();

		CEngine::engine().Render2DQuad(RenderData);

		PROCESS_MEMORY_COUNTERS memCounter;
		bool result = GetProcessMemoryInfo(GetCurrentProcess(),
			&memCounter,
			sizeof(memCounter)) != 0;

		double d = memCounter.WorkingSetSize;

		d /= 1024 * 1024;

		CFonts::fonts().drawTextInScreen(std::to_string(d) + " MB", -0.5, 0.8, 0.05);
		//CFonts::fonts().drawTextInScreen(std::to_string(memCounter.PageFaultCount), 0.0, 0.8, 0.05);


		mainMenu.render();

		GPPGame::GuitarPP().renderFrame();
	}

	CLuaH::Lua().runEvent("atExit");
	CLuaH::Lua().unloadAll();
	return 0;
}

