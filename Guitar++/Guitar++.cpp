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
#include "CFonts.h"
#include "CPlayer.h"
#include "CSaveSystem.h"
#include "CLanguageManager.h"
#include "CGuitars.h"
#include "CCampaing.h"
#include "CGPPFileMGR.h"
#include "CChart.h"
#include "CShader.h"
#include "CControls.h"

#include "CMultiplayer.h"

int main(int argc, char* argv[])
{
	///startGambiarras();
	//try
    {
		CEngine::engine().errorCallbackFun = GPPGame::logError;
		CLog::log();
		CEngine::engine().init();

		auto &lngmgr = CLanguageManager::langMGR();

		auto &game = GPPGame::GuitarPP();
		auto &guitars = CGuitars::inst();

		game.parseParameters(argc, argv);
		
		auto &campaingMgr = CCampaing::campaingMGR();

		auto &lua = CLuaH::Lua();

		// Configuration file
		auto script = lua.newScript(".", "Config.lua");

		if (script.luaState)
		{
			lua.runScript(script);
		}

		// Run all scripts in quere
		lua.runScripts();

		const int mainMenuSettedSE = lua.idForCallbackEvent("mainMenuSetted"), atExitSE = lua.idForCallbackEvent("atExit");

		// Window
		game.createWindow();

		CLog::log().multiRegister("Shaders status: %0", ShaderProject::CShader::inst().isShadersEnabled());

		double ltime = CEngine::engine().getTime();

		int loadLoop = 0;

		bool wOpened = true;

		while ((wOpened = CEngine::engine().windowOpened()) && ((CEngine::engine().getTime() - ltime) < 0.2 || loadLoop < 10))
		{
			if (CEngine::engine().getKey(GLFW_KEY_ESCAPE))
			{
				return 0;
			}

			game.clearScreen();

			switch (loadLoop)
			{
			case 0:
				guitars.loadAllGuitars();
				break;

			case 1:
				game.loadAllThemes();
				game.gThemes["gppdefaulttheme"].apply();
				break;

			default:
				break;
			}

			if (loadLoop != 0x7FFFFFFF)
				++loadLoop;

			game.renderFrame();
		}

		auto &logotext = game.loadTexture("data/sprites", "logo.tga");

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

		loadLoop = 0;

		auto &mainMenu = game.newMenu();
		auto &gmenu = game.newNamedMenu("playOptions");
		auto &cmpopts = game.newNamedMenu("mainCampaingOptions");
		auto &optionsmenu = game.newNamedMenu("optionsmenu");
		auto &audiomenu = game.newNamedMenu("audiomenu");
		auto &extrasmenu = game.newNamedMenu("extrasmenu");
		auto &chartsmenu = game.newNamedMenu("chartsmenu");
		auto &chartCompileMenu = game.newNamedMenu("chartCompileMenu");

		int startOP, configOP, extrasOp, ajudaOp, quitOp, singlePlayOp;

		double proportion = game.getWindowProportion();

		ltime = CEngine::engine().getTime();

		while ((wOpened = CEngine::engine().windowOpened()) && ((CEngine::engine().getTime() - ltime) < 3.0 || loadLoop < 100))
		{
			if (CControls::controls().keyEscape())
			{
				break;
			}

			game.clearScreen();

			{
				switch (loadLoop)
				{
				case 0:
					game.loadBasicSprites();
					break;

				case 1:
					{
					game.loadBasicSounds();

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
							opt.langEntryKey = "menuExtrasTitle";
							opt.y = 0.2;
							opt.x = -proportion + 0.2;
							opt.size = 0.075;
							opt.group = 1;
							opt.status = 0;
							opt.type = CMenu::menusOPT::textbtn;

							opt.menusXRef.push_back(extrasmenu.getName());

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

						{
							CMenu::menuOpt opt;

							opt.text = "aaaa";
							opt.y = -0.3;
							opt.x = -proportion + 0.2;
							opt.size = 0.075;
							opt.group = 1;
							opt.status = 0;
							opt.type = CMenu::menusOPT::text_input;
							opt.preText = "Teste campo Digita\xc3\xa7\xc3\xa3o";

							mainMenu.addOpt(opt);
						}

						{
							CMenu::menuOpt opt;

							opt.text = "Maratona";
							opt.y = 0.4;
							opt.size = 0.075;
							opt.x = CFonts::fonts().getCenterPos(opt.text, opt.size, -0.5);
							opt.group = 1;
							opt.status = 0;
							opt.type = CMenu::menusOPT::textbtn;

							std::string testecallback = game.addGameCallbacks("gamePlaySelectedInMenu", GPPGame::startMarathonModule);
							opt.menusXRef.push_back(testecallback);

							gmenu.addOpt(opt);
						}

						{
							CMenu::menuOpt opt;

							opt.text = "Quick play";
							opt.y = 0.4;
							opt.size = 0.075;
							opt.x = CFonts::fonts().getCenterPos(opt.text, opt.size, 0.0);
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
							opt.x = CFonts::fonts().getCenterPos(opt.text, opt.size, 0.0);
							opt.group = 1;
							opt.status = 0;
							opt.type = CMenu::menusOPT::textbtn;

							opt.menusXRef.push_back(cmpopts.getName());
							gmenu.addOpt(opt);
						}

						{
							CMenu::menuOpt opt;

							opt.text = "Server MP";
							opt.y = 0.2;
							opt.size = 0.075;
							opt.x = CFonts::fonts().getCenterPos(opt.text, opt.size, 0.0);
							opt.group = 1;
							opt.status = 0;
							opt.type = CMenu::menusOPT::textbtn;

							std::string testecallback = game.addGameCallbacks("gamePlayMPSelectedInMenu", GPPGame::serverModule);
							opt.menusXRef.push_back(testecallback);

							gmenu.addOpt(opt);
						}

						{
							CMenu::menuOpt opt;

							opt.text = "Client MP";
							opt.y = 0.1;
							opt.size = 0.075;
							opt.x = CFonts::fonts().getCenterPos(opt.text, opt.size, 0.0);
							opt.group = 1;
							opt.status = 0;
							opt.type = CMenu::menusOPT::textbtn;

							std::string testecallback = game.addGameCallbacks("patosVoadores", GPPGame::testClient);
							opt.menusXRef.push_back(testecallback);

							gmenu.addOpt(opt);
						}


						{
							CMenu::menuOpt opt;

							opt.text = "Voltar";
							opt.y = 0.0;
							opt.size = 0.075;
							opt.x = CFonts::fonts().getCenterPos(opt.text, opt.size, 0.0);
							opt.group = 1;
							opt.status = 0;
							opt.type = CMenu::menusOPT::textbtn;
							opt.goback = true;

							gmenu.addOpt(opt);
						}


						//////////////////***********************************************************

						campaingMgr.campaingMainMenu(cmpopts);

						//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
					}

				case 2:
					{
						{
							CMenu::menuOpt opt;

							opt.text = "Graficos";
							opt.y = 0.4;
							opt.size = 0.075;
							opt.x = CFonts::fonts().getCenterPos(opt.text, opt.size, 0.0);
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
							opt.x = CFonts::fonts().getCenterPos(opt.text, opt.size, 0.0);
							opt.group = 1;
							opt.status = 0;
							opt.type = CMenu::menusOPT::textbtn;

							opt.menusXRef.push_back(audiomenu.getName());

							optionsmenu.addOpt(opt);
						}

						{
							CMenu::menuOpt opt;

							opt.text = "Gameplay";
							opt.y = 0.2;
							opt.size = 0.075;
							opt.x = CFonts::fonts().getCenterPos(opt.text, opt.size, 0.0);
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
							opt.x = CFonts::fonts().getCenterPos(opt.text, opt.size, 0.0);
							opt.group = 1;
							opt.status = 0;
							opt.type = CMenu::menusOPT::textbtn;
							opt.goback = true;

							optionsmenu.addOpt(opt);
						}
					}
					break;

				case 3:
					{
						{
							CMenu::menuOpt opt;

							opt.text = "Volume principal";
							opt.y = 0.3;
							opt.size = 0.075;
							opt.x = CFonts::fonts().getCenterPos(opt.text, opt.size, 0.0);
							opt.group = 1;
							opt.status = 0;
							opt.type = CMenu::menusOPT::deslizant_Select_list;
							opt.deslizantBarSize = 0.5;

							opt.optList = { "0.0", "0.1", "0.2", "0.3", "0.4", "0.5", "0.6", "0.7", "0.8", "0.9", "1.0" };

							opt.listID = static_cast<int>((opt.optList.size() - 1) * CEngine::getMainVolume());

							if (opt.listID < 0)
								opt.listID = 0;

							if (opt.listID >= static_cast<int>(opt.optList.size()))
								opt.listID = opt.optList.size() - 1;

							opt.updateCppCallback = [](CMenu &menu, CMenu::menuOpt &opt)
							{
								if (opt.status == 0)
								{
									try
									{
										opt.listID = static_cast<int>((opt.optList.size() - 1) * CEngine::getMainVolume());

										if (opt.listID < 0)
											opt.listID = 0;

										if (opt.listID >= static_cast<int>(opt.optList.size()))
											opt.listID = opt.optList.size() - 1;
									}
									catch (...)
									{

									}
								}

								return 0;
							};

							opt.posUpdateCppCallback = [](CMenu &menu, CMenu::menuOpt &opt)
							{
								if (opt.status != 0)
								{
									auto &selectedVolume = opt.optList[opt.listID];

									try
									{
										CEngine::setMainVolume(std::stof(selectedVolume));
									}
									catch (...)
									{

									}
								}

								return 0;
							};

							audiomenu.addOpt(opt);
						}

						{
							CMenu::menuOpt opt;

							opt.text = "Voltar";
							opt.y = -0.4;
							opt.size = 0.075;
							opt.x = CFonts::fonts().getCenterPos(opt.text, opt.size, 0.0);
							opt.group = 1;
							opt.status = 0;
							opt.type = CMenu::menusOPT::textbtn;
							opt.goback = true;

							audiomenu.addOpt(opt);
						}
					}

					game.setMainMenu(mainMenu);
					lua.runEvent(mainMenuSettedSE);
					break;

				case 4:
					{
						auto &menu = game.loadTexture("data/sprites", "menu.tga");
						game.uiRenameMenu.qbgd.Text = game.loadTexture("data/sprites", "interfacebg.tga").getTextId();
						game.uiCreateProfile.qbgd.Text = game.uiRenameMenu.qbgd.Text;
					}
					break;

				case 5:
				{
					{
						CMenu::menuOpt opt;

						opt.text = "Charts";
						opt.y = 0.4;
						opt.x = -proportion + 0.2;
						opt.size = 0.075;
						opt.group = 1;
						opt.status = 0;
						opt.type = CMenu::menusOPT::textbtn;

						opt.menusXRef.push_back(chartsmenu.getName());

						extrasmenu.addOpt(opt);
					}

					{
						CMenu::menuOpt opt;

						opt.text = "Voltar";
						opt.y = 0.0;
						opt.size = 0.075;
						opt.x = CFonts::fonts().getCenterPos(opt.text, opt.size, 0.0);
						opt.group = 1;
						opt.status = 0;
						opt.type = CMenu::menusOPT::textbtn;
						opt.goback = true;

						extrasmenu.addOpt(opt);
					}
				}


				{
					{
						CMenu::menuOpt opt;

						opt.text = "Compilador de .chart para .gpp";
						opt.y = 0.4;
						opt.x = -proportion + 0.2;
						opt.size = 0.075;
						opt.group = 1;
						opt.status = 0;
						opt.type = CMenu::menusOPT::textbtn;

						opt.menusXRef.push_back(chartCompileMenu.getName());

						chartsmenu.addOpt(opt);
					}

					{
						CMenu::menuOpt opt;

						opt.text = "Criador de charts";
						opt.y = 0.3;
						opt.x = -proportion + 0.2;
						opt.size = 0.075;
						opt.group = 1;
						opt.status = 0;
						opt.type = CMenu::menusOPT::textbtn;

						std::string testecallback = game.addGameCallbacks("gameCharter", GPPGame::charterModule);
						opt.menusXRef.push_back(testecallback);

						chartsmenu.addOpt(opt);
					}

					{
						CMenu::menuOpt opt;

						opt.text = "Voltar";
						opt.y = 0.0;
						opt.size = 0.075;
						opt.x = CFonts::fonts().getCenterPos(opt.text, opt.size, 0.0);
						opt.group = 1;
						opt.status = 0;
						opt.type = CMenu::menusOPT::textbtn;
						opt.goback = true;

						chartsmenu.addOpt(opt);
					}
				}


				{
					{
						CMenu::menuOpt opt;

						opt.text = "Voltar";
						opt.y = 0.85;
						opt.size = 0.075;
						opt.x = -1.0;
						opt.group = 1;
						opt.status = 0;
						opt.type = CMenu::menusOPT::textbtn;
						opt.goback = true;

						chartCompileMenu.addOpt(opt);
					}
				}
					campaingMgr.loadCampaingModes();
					break;

				case 6:
					game.initialLoad();
					break;

				case 7:
					game.initialLoad2();
					break;

				default:
					break;
				}

				if (loadLoop != 0x7FFFFFFF)
					++loadLoop;
			}


			double t = (CEngine::engine().getTime() - ltime) * 3.14 / 3.0;

			CEngine::engine().setColor(1.0, 1.0, 1.0, sin(t));
			CEngine::engine().Render2DQuad(lgrdata);

			CEngine::engine().setColor(1.0, 1.0, 1.0, sin(t + 0.1));
			CFonts::fonts().drawTextInScreenWithBuffer(str2nibblePresents, CFonts::fonts().getCenterPos(str2nibblePresents, 0.1, 0.0), -0.6, 0.1);
			

			CEngine::engine().setColor(1.0, 1.0, 1.0, 1.0);
			game.renderFrame();
		}

		if (!wOpened)
		{
			return 0;
		}

		GPPGame::GuitarPP().setVSyncMode(1);

		/*CMultiplayer mp;

		std::cout << "Trying to start mp\n\n";
		mp.startServer("7777");
		std::this_thread::sleep_for(std::chrono::seconds(1));
		mp.clientConnect("127.0.0.1", "7777");*/

		{
			//CCampaing::campaingMGR().loadedCampaingFilepath = "./data/saves/campaings/campaingZoeira/save";
			cfile_ptr svfstream(CEngine::make_cfile("./data/saves/campaings/campaingZoeira/save", "rb"));

			if (svfstream)
			{
				svfstream.reset();

				CCampaing::campaingMGR().loadCampaingF("./data/saves/campaings/campaingZoeira/save");
			}
			else
			{
				svfstream.reset();

				CCampaing::campaingMGR().newCampaing();
			}
		}

		game.selectPlayerMenu();
		game.openMenus(game.getMainMenu(), nullptr, nullptr, nullptr, false);

		lua.runEvent(atExitSE);
		guitars.unload();
		lua.unloadAll();
	}// catch (const std::exception &e)
	//{
	//	CLog::log() << e.what();
	//}
	return 0;
}

