#include "CMenu.h"
#include "CEngine.h"
#include "CFonts.h"
#include "GPPGame.h"
#include <iostream>

std::unordered_map <std::string, CMenu*> CMenu::Menus = std::unordered_map <std::string, CMenu*>();
int CMenu::menusCreated = 0;

int CMenu::addOpt(const menuOpt &opt){
	options.push_back(opt);

	groupInfo[opt.group] = group();

	return options.size() - 1;
}

void CMenu::resetData(){
	for (auto &opt : options){
		opt.status = 0;
	}
}

void CMenu::render(){
	auto textSizeInScreen = [](double charnums, double size){
		return (charnums * size / 1.5) + (size / 2.0);
	};

	for (auto &grp : groupInfo){
		switch (grp.second.type){
		case group::deslizant_h_menu:
			CEngine::RenderDoubleStruct RenderData;

			RenderData.Text = 0;

			RenderData.x1 = grp.second.x;
			RenderData.x2 = RenderData.x1 + grp.second.w;
			RenderData.x3 = RenderData.x1 + grp.second.w;
			RenderData.x4 = grp.second.x;

			RenderData.y1 = grp.second.y + grp.second.h;
			RenderData.y2 = RenderData.y1 + grp.second.h;
			RenderData.y3 = RenderData.y1;
			RenderData.y4 = grp.second.y;

			RenderData.TextureX1 = 0.0;
			RenderData.TextureX2 = 1.0;
			RenderData.TextureY1 = 1.0;
			RenderData.TextureY2 = 0.0;

			CEngine::engine().Render2DQuad(RenderData);
			break;
		}
	}

	for (auto &opt : options){
		switch (opt.type){
		case button_ok:
			break;

		case text_input:
			break;

		case textbtn:
			CEngine::engine().setColor(opt.color[0], opt.color[1], opt.color[2], opt.color[3]);
			CFonts::fonts().DrawTextInGLFWWindow(opt.text, opt.x, opt.y, opt.size);
			if (opt.status & 1){
				CEngine::RenderDoubleStruct RenderData;
				RenderData.x1 = opt.x - opt.size / 2.0;
				RenderData.x2 = opt.x - opt.size / 2.0 + opt.size;
				RenderData.x3 = opt.x - opt.size / 2.0 + opt.size;
				RenderData.x4 = opt.x - opt.size / 2.0;

				RenderData.y1 = opt.y + opt.size;
				RenderData.y2 = opt.y + opt.size;
				RenderData.y3 = opt.y;
				RenderData.y4 = opt.y;

				RenderData.Text = GPPGame::GuitarPP().SPR["palheta"];

				RenderData.TextureX1 = 0.0;
				RenderData.TextureX2 = 1.0;
				RenderData.TextureY1 = 1.0;
				RenderData.TextureY2 = 0.0;

				CEngine::engine().Render2DQuad(RenderData);
			}
			break;

		case deslizant_Select_list:
			CEngine::engine().setColor(opt.color[0], opt.color[1], opt.color[2], opt.color[3]);
			CFonts::fonts().DrawTextInGLFWWindow(opt.text, opt.x, opt.y, opt.size);

			double textSize = textSizeInScreen(opt.text.size(), opt.size);
			double barPosX1 = opt.x + textSize + opt.size, barPosX2;
			barPosX2 = barPosX1 + opt.deslizantBarSize;
			double barPosXStep = opt.deslizantBarSize / opt.optList.size();
			barPosXStep *= opt.listID;


			{
				CEngine::RenderDoubleStruct RenderData;

				RenderData.x1 = barPosX1;
				RenderData.x2 = barPosX1 + opt.deslizantBarSize;
				RenderData.x3 = barPosX1 + opt.deslizantBarSize;
				RenderData.x4 = barPosX1;

				RenderData.y1 = opt.y + opt.size;
				RenderData.y2 = opt.y + opt.size;
				RenderData.y3 = opt.y;
				RenderData.y4 = opt.y;

				RenderData.z1 = 0.0;
				RenderData.z2 = 0.0;
				RenderData.z3 = 0.0;
				RenderData.z4 = 0.0;

				RenderData.TextureX1 = 0.0;
				RenderData.TextureX2 = 1.0;
				RenderData.TextureY1 = 1.0;
				RenderData.TextureY2 = 0.0;

				RenderData.Text = 0;

				CEngine::engine().Render2DQuad(RenderData);
			}

			double absolutePos = barPosX1 + barPosXStep;

			CEngine::RenderDoubleStruct RenderData;
			RenderData.x1 = absolutePos;
			RenderData.x2 = absolutePos + opt.size;
			RenderData.x3 = absolutePos + opt.size;
			RenderData.x4 = absolutePos;

			RenderData.y1 = opt.y + opt.size;
			RenderData.y2 = opt.y + opt.size;
			RenderData.y3 = opt.y;
			RenderData.y4 = opt.y;

			RenderData.Text = GPPGame::GuitarPP().SPR["palheta"];

			RenderData.TextureX1 = 0.0;
			RenderData.TextureX2 = 1.0;
			RenderData.TextureY1 = 1.0;
			RenderData.TextureY2 = 0.0;

			CEngine::engine().Render2DQuad(RenderData);

			barPosX1 += opt.deslizantBarSize + opt.size;

			CFonts::fonts().DrawTextInGLFWWindow(opt.optList[opt.listID], barPosX1, opt.y, opt.size);
			break;
		}
	}
	CEngine::engine().setColor(1.0, 1.0, 1.0, 1.0);
}

void CMenu::update(){
	CLuaH::multiCallBackParams_t menucallback;
	menucallback.push_back(CLuaH::customParam(menuName));

	CLuaH::Lua().runEventWithParams(std::string("pre") + menuName + std::string("update"), menucallback);

	auto textSizeInScreen = [](double charnums, double size){
		return (charnums * size / 1.5) + (size / 2.0);
	};

	auto isMouseOver2DQuad = [](double x, double y, double w, double h){
		double mx = CEngine::engine().mouseX, my = CEngine::engine().mouseY;
		return mx >= x && mx <= x + w && my >= y - h && my <= y;
	};

	auto desselectAllFromGroup = [this](int group){
		if (group < 0) return;
		for (auto &opt : options){
			if (opt.group == group){
				opt.status &= ~1;
			}
		}
	};

	int leftKey = CEngine::engine().getKey(GLFW_KEY_LEFT), rightKey = CEngine::engine().getKey(GLFW_KEY_RIGHT);

	bool enterOpt = false, mBTNClick = false;

	if (CEngine::engine().getMouseButton(0)){
		mBTNClick = true;
	}

	if (CEngine::engine().getKey(GLFW_KEY_ENTER) || mBTNClick){
		enterOpt = status = true;
	}

	for (auto &grp : groupInfo){
		switch (grp.second.type){
		case group::deslizant_h_menu:
			if (isMouseOver2DQuad(grp.second.x, grp.second.y, grp.second.w, grp.second.h)){


			}
		}
	}

	double textSize = 0.0;
	double barPosX1 = 0.0;
	double barPosX2 = 0.0;

	int i = 0;
	for (auto &opt : options){
		std::string optCallBackTXT = menuName + std::string("_") + std::to_string((int)(opt.x * 100.0)) + std::to_string((int)(opt.y * 100.0)) + std::string("g") + std::to_string((int)(opt.group));

		CLuaH::multiCallBackParams_t params;

		params.push_back(CLuaH::customParam(menuName));
		params.push_back(CLuaH::customParam(optCallBackTXT));
		params.push_back(CLuaH::customParam(opt.optValue));

		if (opt.shortcutKey){
			int pressing = CEngine::engine().getKey(opt.shortcutKey);
			if (opt.pressingShortcutKey && !pressing){
				desselectAllFromGroup(opt.group);
				opt.status |= 1 | 2;
				opt.pressingShortcutKey = false;
			}

			if (pressing){
				opt.pressingShortcutKey = true;

				params.push_back(CLuaH::customParam(2.0));
			}
		}

		switch (opt.type){
		case button_ok:

			break;

		case textbtn:
			textSize = textSizeInScreen(opt.text.size(), opt.size);

			if (isMouseOver2DQuad(opt.x - (opt.size * 0.05), opt.y, textSize, opt.size * 1.05)){
				desselectAllFromGroup(opt.group);
				opt.status = enterOpt ? 1 | 2 : 1;

				if (params.size() <= 3) params.push_back(CLuaH::customParam((double)opt.status));

				if (opt.group >= 0){
					groupInfo[opt.group].selectedOpt = i;
				}
			}
			else{
				opt.status = opt.status & 1;
			}
			break;

		case deslizant_Select_list:
			textSize = textSizeInScreen(opt.text.size(), opt.size);
			barPosX1 = opt.x + textSize + opt.size;
			barPosX2 = barPosX1 + opt.deslizantBarSize;

			if (isMouseOver2DQuad(barPosX1, opt.y, opt.deslizantBarSize, opt.size)){
				if (mBTNClick){
					double subX = (CEngine::engine().mouseX - barPosX1);
					opt.listID = (int)(((double)opt.optList.size() / opt.deslizantBarSize) * subX);

					if (opt.listID >= opt.optList.size()){
						opt.listID = opt.optList.size() - 1;
					}
					else if(opt.listID < 0){
						opt.listID = 0;
					}
					if (params.size() <= 3) params.push_back(CLuaH::customParam(2.0));
				}
			}
			break;
		}

		if (params.size() <= 3) params.push_back(CLuaH::customParam(-1.0));
		CLuaH::Lua().runEventWithParams(optCallBackTXT, params);
		++i;
	}
	CLuaH::Lua().runEventWithParams(std::string("pos") + menuName + std::string("update"), menucallback);
}

CMenu::CMenu(){
	status = 0;

	++menusCreated;
	lScript = nullptr;

	menuName = "menu_main_";
	menuName += std::to_string(menusCreated);

	Menus[menuName] = this;
}

CMenu::CMenu(const std::string &name){
	status = 0;

	++menusCreated;
	lScript = nullptr;

	menuName = "menu_";
	menuName += name;
	menuName += "_";
	menuName += std::to_string(menusCreated);
	Menus[menuName] = this;
}

CMenu::~CMenu(){
	Menus.erase(menuName);
}
