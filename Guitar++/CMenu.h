#pragma once
#ifndef __GUITARPP_CMENU_H_
#define __GUITARPP_CMENU_H_

#include <deque>
#include <map>
#include <string>
#include <iostream>
#include <cstdint>
#include "CLuaH.hpp"
#include "CEngine.h"

class CMenu{
	std::string menuName;
	static int menusCreated;
	static std::map <std::string, CMenu*> Menus;

	static std::string textBuffer;

	struct uiWindowStruct
	{
		CMenu *m;
		int pos;

		uiWindowStruct()
		{
			m = nullptr;
			pos = 0;
		}
	};

	struct posUiOrder
	{
		int id;
		int pos;

		bool operator>(const posUiOrder &p0) const
		{
			return pos > p0.pos;
		}

		bool operator<(const posUiOrder &p0) const
		{
			return pos < p0.pos;
		}

		bool operator==(const posUiOrder &p0) const
		{
			return pos == p0.pos;
		}
	};

	static std::deque<uiWindowStruct> uiList;
	static std::vector<posUiOrder> uiOrderList;
	std::deque<int> myUiList;

	static int allocOrGetUiFreeSpace();

	bool uiMenu;
	int thisUiID;

	static void refreshMenusUiPosOrder();
	static void interfaceCalcPos();

	posUiOrder &getThisOnUiOrder();
	posUiOrder &getTopUI();
	void putOnTop();
	bool isThisOnTop();

public:
	bool isInterfaceOver();

	int getUIListSize();

	bool isMouseOnThisMenu();

	static inline uiWindowStruct &getUiAt(int index)
	{
		return uiList[index];
	}

	static void renderUiList();

	inline uiWindowStruct &getUILast()
	{
		if (uiList.size() > 0)
			return uiList.back();

		return uiWindowStruct();
	}

	double x, y;

	bool devMenuNOUpdateOthers;

	enum menusOPT{button_ok, text_input, button_cancel, textbtn, multi_btn, deslizant_Select_list, button_ui, drag_bar, static_text};
	std::string mTitle;

	CLuaH::luaScript *lScript;
	std::map <std::string, int> luaCallBacks;

	CEngine::RenderDoubleStruct qbgd;

	std::string backgroundTexture;
	CMenu *devEditMenu;
	int devEditingOpt;

	bool temp, gameMenu;

	double mouseAX;
	double mouseAY;

	struct menuOpt{
		menusOPT type;
		int group;
		unsigned int texture;
		std::string optionName;

		bool goback;

		bool enableEnter;

		int pressingShortcutKey;
		int shortcutKey;

		std::string text, preText, langEntryKey;
		std::string *externalPreTextRef;
		int preTextMaxSize;
		int status;
		int devStatus;
		double optValue, deslizantBarSize;
		int listID;

		bool btnClickStat;

		CLuaH::luaScript *attachedLua;

		std::deque<std::string> optList;

		std::deque < std::string > menusXRef;

		double x, y, size;
		int strEditPoint;

		double color[4];

		std::function<int(menuOpt&)> updateCppCallback;

		void update();

		inline menuOpt()
		{
			type = button_ok;
			group = -1;
			status = 0;
			x = y = 0.0;
			size = 0.1;
			texture = 0;
			deslizantBarSize = 0.0;
			listID = 0;
			optValue = 0;
			devStatus = 0;
			preTextMaxSize = 30;

			attachedLua = nullptr;
			externalPreTextRef = nullptr;
			strEditPoint = 0;

			goback = false;
			btnClickStat = false;

			enableEnter = true;

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

	std::map<int, group> groupInfo;
	std::deque<menuOpt> options;

	std::function <int (CMenu&)> openCallback;

	int pushUserInterface(const CMenu &m);

	int getDevSelectedMenuOpt();

	void update();
	void updateDev();

	void render();
	int addOpt(const menuOpt &opt);

	int getNumOfOptions()
	{
		return options.size();
	}

	std::string getNameFromOpt(int optID)
	{
		return options[optID].optionName;
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
