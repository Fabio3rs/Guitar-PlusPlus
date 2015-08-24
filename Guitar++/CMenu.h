#pragma once
#ifndef __GUITARPP_CMENU_H_
#define __GUITARPP_CMENU_H_

#include <deque>
#include <map>
#include <string>
#include <iostream>
#include <unordered_map>
#include <cstdint>
#include "CLuaH.hpp"

class CMenu{
	std::string menuName;
	static int menusCreated;
	static std::unordered_map <std::string, CMenu*> Menus;

public:
	enum menusOPT{button_ok, text_input, button_cancel, textbtn, multi_btn, deslizant_Select_list};
	std::string mTitle;

	CLuaH::luaScript *lScript;
	std::unordered_map <std::string, int> luaCallBacks;

	std::string backgroundTexture;

	bool temp, gameMenu;

	struct menuOpt{
		menusOPT type;
		int group;
		unsigned int texture;

		bool goback;

		int pressingShortcutKey;
		int shortcutKey;

		std::string text, preText;
		int status;
		double optValue, deslizantBarSize;
		int listID;

		CLuaH::luaScript *attachedLua;

		std::deque<std::string> optList;

		std::deque < std::string > menusXRef;

		double x, y, size;

		double color[4];

		inline menuOpt(){
			type = button_ok;
			group = -1;
			status = 0;
			x = y = 0.0;
			size = 0.1;
			texture = 0;
			deslizantBarSize = 0.0;
			listID = 0;
			optValue = 0;

			attachedLua = nullptr;

			goback = false;

			pressingShortcutKey = shortcutKey = 0;

			color[0] = color[1] = color[2] = color[3] = 1.0;

			preText = "type here";
		}
	};

	struct group{
		enum types{none, deslizant_h_menu};
		double x, y, h, w;
		double nx, ny;
		int status;
		int selectedOpt;
		int type;

		group(){
			nx = ny = x = y = h = w = 0.0;
			status = 0;
			selectedOpt = 0;
			type = 0;
		}
	};

	inline const std::string &getName()
	{
		return menuName;
	}

	std::unordered_map<int, group> groupInfo;
	std::deque<menuOpt> options;

	void update();

	void render();
	int addOpt(const menuOpt &opt);

	int getNumOfOptions()
	{
		return options.size();
	}

	std::string getNameFromOpt(int optID)
	{
		return menuName + std::string("_") + std::to_string((int)(options[optID].x * 100.0)) + std::to_string((int)(options[optID].y * 100.0)) + std::string("g") + std::to_string((int)(options[optID].group));
	}

	void resetData();

	int status;

	CMenu();
	CMenu(const std::string &name);

	/*static inline CMenu *getMenuByName(const std::string &n)
	{
		return Menus[n];
	}*/

	~CMenu();
};


#endif
