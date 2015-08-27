#include "CLuaFunctions.hpp"
#include "GPPGame.h"
#include "CFonts.h"
#include <iostream>
#include "CLog.h"

CLuaFunctions &CLuaFunctions::LuaF()
{
	static CLuaFunctions LuaF;
	return LuaF;
}

CLuaFunctions::LuaParams::LuaParams(lua_State *state)
{
	L = state;
	num_params = lua_gettop(L);

	ret = 0;
	stck = 1;
	fail_bit = 0;
}

CLuaFunctions::LuaParams &CLuaFunctions::LuaParams::operator<<(const std::string &param)
{
	lua_pushstring(L, param.c_str());
	++ret;

	return *this;
}

CLuaFunctions::LuaParams &CLuaFunctions::LuaParams::operator<<(double param)
{
	lua_pushnumber(L, param);
	++ret;

	return *this;
}

CLuaFunctions::LuaParams &CLuaFunctions::LuaParams::operator<<(int param)
{
	lua_pushinteger(L, param);
	++ret;

	return *this;
}

CLuaFunctions::LuaParams &CLuaFunctions::LuaParams::operator<<(size_t param)
{
	lua_pushunsigned(L, param);
	++ret;

	return *this;
}

CLuaFunctions::LuaParams &CLuaFunctions::LuaParams::operator<<(bool param)
{
	lua_pushboolean(L, param);
	++ret;

	return *this;
}

CLuaFunctions::LuaParams &CLuaFunctions::LuaParams::operator>>(std::string &param)
{
	if (stck <= num_params){
		param = lua_tostring(L, stck);
		++stck;
	}
	else
	{
		fail_bit = 1;
	}

	return *this;
}

CLuaFunctions::LuaParams &CLuaFunctions::LuaParams::operator>>(double &param)
{
	if (stck <= num_params){
		param = lua_tonumber(L, stck);
		++stck;
	}
	else
	{
		fail_bit = 1;
	}

	return *this;
}

CLuaFunctions::LuaParams &CLuaFunctions::LuaParams::operator>>(int &param)
{
	if (stck <= num_params){
		param = lua_tointeger(L, stck);
		++stck;
	}
	else
	{
		fail_bit = 1;
	}

	return *this;
}

CLuaFunctions::LuaParams &CLuaFunctions::LuaParams::operator>>(void *&param)
{
	if (stck <= num_params){
		param = (void*)lua_tounsigned(L, stck);
		++stck;
	}
	else
	{
		fail_bit = 1;
	}

	return *this;
}

CLuaFunctions::LuaParams &CLuaFunctions::LuaParams::operator>>(bool &param)
{
	if (stck <= num_params){
		param = lua_toboolean(L, stck) != 0;
		++stck;
	}
	else
	{
		fail_bit = 1;
	}

	return *this;
}

bool CLuaFunctions::LuaParams::fail()
{
	return fail_bit != 0;
}

int CLuaFunctions::LuaParams::rtn()
{
	return ret;
}

int CLuaFunctions::LuaParams::getNumParams()
{
	return num_params;
}

int CLuaFunctions::setConfigs(lua_State *L)
{
	auto cfg = GPPGame::GuitarPP().getWindowDefaults();


	lua_getglobal(L, "WindowWidth");

	if (lua_isnumber(L, -1))
		cfg.w = lua_tointeger(L, -1);



	lua_getglobal(L, "WindowHeight");

	if (lua_isnumber(L, -1))
		cfg.h = lua_tointeger(L, -1);



	lua_getglobal(L, "FullScreen");

	if (lua_isboolean(L, -1))
		cfg.fullscreen = lua_toboolean(L, -1) != 0;


	lua_getglobal(L, "customTitle");

	if (lua_isstring(L, -1))
		cfg.name = lua_tostring(L, -1);



	lua_getglobal(L, "VSync");

	if (lua_isnumber(L, -1))
		cfg.VSyncMode = lua_tointeger(L, -1);


	GPPGame::GuitarPP().settWindowConfigs(cfg);

	return 0;
}

/*
*
*/
int CLuaFunctions::newGamePlayModule(lua_State *L)
{
	LuaParams p(L);

	if (p.getNumParams() == 1 && lua_isstring(L, 1)){
		std::string name;

		p >> name;

		auto &module = GPPGame::GuitarPP().gameModules[name];

		lua_getglobal(L, "backgroundTexture");

		if (lua_isstring(L, -1))
			module.backgroundTexture = lua_tostring(L, -1);


		lua_getglobal(L, "fretsTextures");

		if (lua_isstring(L, -1))
			module.fretsTextures = lua_tostring(L, -1);


		lua_getglobal(L, "musicSpeed");

		if (lua_isnumber(L, -1))
			module.setMusicSpeed(lua_tonumber(L, -1));


		lua_getglobal(L, "hyperSpeed");

		if (lua_isnumber(L, -1))
			module.setHyperSpeed(lua_tonumber(L, -1));
	}

	return p.rtn();

}

/*
* Load a texture (load file & load opengl texture)
*/
int CLuaFunctions::loadTexture(lua_State *L)
{
	LuaParams p(L);

	for (int i = 0; i < p.getNumParams(); i++){
		if (lua_isstring(L, i + 1)){
			std::string str;
			p >> str;

			unsigned int result = 0;

			result = GPPGame::GuitarPP().loadTexture("data/sprites", str, &CLuaH::Lua().getLastScript()).getTextId();

			if (result){
				p << result;
			}
			else{
				p << false;
			}
		}
		else{
			p << false;
		}
	}

	return p.rtn();
}

int CLuaFunctions::doNotRunAgain(lua_State *L)
{
	CLuaH::Lua().getLastScript().runAgain = false;

	return 0;
}

/*
setCallBackToEvent(event, function)
*/
int CLuaFunctions::setCallBackToEvent(lua_State *L)
{
	LuaParams p(L);

	if (p.getNumParams() == 2 && lua_isstring(L, 1) && lua_isfunction(L, 2)){
		std::string eventName = "teste";
		eventName = lua_tostring(L, 1);

		lua_pushvalue(L, 2);
		int	fnRef = luaL_ref(L, LUA_REGISTRYINDEX);

		CLuaH::Lua().getLastScript().callbacks[eventName] = fnRef;
	}


	return p.rtn();
}

/**/
int CLuaFunctions::drawTextOnScreen(lua_State *L)
{
	LuaParams p(L);

	if (p.getNumParams() == 4 && lua_isstring(L, 1) && lua_isnumber(L, 2) && lua_isnumber(L, 3) && lua_isnumber(L, 4)){
		CFonts::fonts().drawTextInScreen(lua_tostring(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4));
	}
	return p.rtn();
}

/*
* New menu
*/
int CLuaFunctions::newMenu(lua_State *L)
{
	LuaParams p(L);

	if (p.getNumParams() == 1 && lua_isstring(L, 1)){
		std::string str;
		p >> str;
		
		if (str.size() > 0)
			str = std::to_string(rand());

		auto menu = GPPGame::GuitarPP().newNamedMenu(str);
		p << menu.getName();
	}
	return p.rtn();
}


/*
*
*/
int CLuaFunctions::assingGameFunctionToMenuOption(lua_State *L)
{
	LuaParams p(L);

	if (p.getNumParams() == 3 && lua_isstring(L, 1) && lua_isnumber(L, 2) && lua_isstring(L, 3))
	{
		std::string menuName, gameFunction;
		int optID;

		p >> menuName;
		p >> optID;
		p >> gameFunction;

		auto &menu = GPPGame::GuitarPP().getMenuByName(menuName);

		menu.options[optID].menusXRef.push_back(gameFunction);
	}
	return p.rtn();
}

/*
*
*/
int CLuaFunctions::assingMenuToOtherMenuOption(lua_State *L)
{
	LuaParams p(L);

	if (p.getNumParams() == 3 && lua_isstring(L, 1) && lua_isnumber(L, 2) && lua_isstring(L, 3))
	{
		std::string menuName, targetMenuName;
		int optID;

		p >> menuName;
		p >> optID;
		p >> targetMenuName;

		auto &menu = GPPGame::GuitarPP().getMenuByName(menuName);

		menu.options[optID].menusXRef.push_back(targetMenuName);
	}
	return p.rtn();
}

/*
* Output a string in game log
*/
int CLuaFunctions::printTolog(lua_State *L)
{
	LuaParams p(L);

	std::string str = "Lua script log: ";

	for (int i = 1, size = p.getNumParams(); i <= size; i++){
		switch (lua_type(L, i))
		{
		case LUA_TNIL:
			str += " <null> ";
			break;

		case LUA_TNUMBER:
			str += " Number<";
			str += std::to_string(lua_tonumber(L, i));
			str += "> ";
			break;

		case LUA_TBOOLEAN:
			str += ((lua_toboolean(L, i) != 0)? " Boolean<True> " : " Boolean<False> ");
			break;

		case LUA_TSTRING:
			str += " String<";
			str += lua_tostring(L, i);
			str += "> ";
			break;

		case LUA_TTABLE:
			str += " <Get LUA_TTABLE: **TODO**> ";
			break;

		case LUA_TFUNCTION:
			str += " <Get LUA_TFUNCTION: **TODO**> ";
			break;

		case LUA_TUSERDATA:
			str += " <Get LUA_TUSERDATA: **TODO**> ";
			break;

		case LUA_TTHREAD:
			str += " <Get LUA_TTHREAD: **TODO**> ";
			break;

		case LUA_TLIGHTUSERDATA:
			str += " <Get LUA_TLIGHTUSERDATA: **TODO**> ";
			break;

		default:
			str += " <unknow argment type> ";
			break;
		}
	}
	CLog::log() << str;

	return 0;
}

/*
* New menu option
*/
int CLuaFunctions::newMenuOption(lua_State *L)
{
	LuaParams p(L);

	if (p.getNumParams() == 8 && lua_isstring(L, 1) && lua_isstring(L, 2) && lua_isnumber(L, 3) && lua_isnumber(L, 4) && lua_isnumber(L, 5)
		&& lua_isnumber(L, 6) && lua_isnumber(L, 7) && lua_isfunction(L, 8)){
		std::string menuName;
		int optID = 0;

		p >> menuName;

		CMenu::menuOpt opt;

		p >> opt.text;
		p >> opt.x;
		p >> opt.y;
		p >> opt.size;
		p >> opt.group;
		p >> opt.type;

		if (opt.type == -1)
		{
			opt.type = CMenu::menusOPT::textbtn;
			opt.goback = true;
		}

		p << (optID = GPPGame::GuitarPP().getMenuByName(menuName).addOpt(opt));

		// ************************************************************** auto callback system
		lua_pushvalue(L, 8);
		int	fnRef = luaL_ref(L, LUA_REGISTRYINDEX);

		// event menuOptionName
		CLuaH::Lua().getLastScript().callbacks[GPPGame::GuitarPP().getMenuByName(menuName).getNameFromOpt(optID)] = fnRef;
	}
	return p.rtn();
}

/*
*
*/
int CLuaFunctions::getMenuOptionName(lua_State *L)
{
	LuaParams p(L);

	if (p.getNumParams() == 2 && lua_isstring(L, 1) && lua_isnumber(L, 2)){
		std::string menuName;
		int optID = 0;

		p >> menuName;
		p >> optID;

		p << GPPGame::GuitarPP().getMenuByName(menuName).getNameFromOpt(optID);
	}

	return p.rtn();
}


/*
*
*/
int CLuaFunctions::getNumOfMenuOptions(lua_State *L)
{
	LuaParams p(L);

	if (p.getNumParams() == 1 && lua_isstring(L, 1)){
		std::string menuName;

		p >> menuName;

		p << GPPGame::GuitarPP().getMenuByName(menuName).getNumOfOptions();
	}

	return p.rtn();
}

/*
*
*/
int CLuaFunctions::setMenuBackgroundTexture(lua_State *L)
{
	LuaParams p(L);

	if (p.getNumParams() == 3 && lua_isstring(L, 1) && lua_isstring(L, 2) && lua_isstring(L, 3)){
		auto &game = GPPGame::GuitarPP();

		std::string menuName, texturePath, textureName;

		p >> menuName;
		p >> texturePath;
		p >> textureName;

		game.getMenuByName(menuName).backgroundTexture = game.loadTexture(texturePath, textureName).getGTextureName();
	}

	return p.rtn();
}

/*
*
*/
int CLuaFunctions::getMainMenuName(lua_State *L)
{
	LuaParams p(L);

	CMenu *main = GPPGame::GuitarPP().getMainMenu();

	if (main != nullptr){
		p << main->getName();
	}
	else{
		p << false;
	}

	return p.rtn();
}

int CLuaFunctions::addTextureToFont(lua_State *L)
{
	LuaParams p(L);

	auto to_wstring = [](const std::string &s){
		std::wstring result(s.begin(), s.end());
		return result;
	};

	if (p.getNumParams() == 4 && lua_isstring(L, 1) && lua_isstring(L, 2) && lua_isstring(L, 3) && lua_isstring(L, 4)){
		p << CFonts::fonts().addTextureToFont(lua_tostring(L, 1), lua_tostring(L, 2), lua_tostring(L, 3), lua_tostring(L, 4));
	}

	return p.rtn();
}

/*
*
*/
int CLuaFunctions::getActualMenu(lua_State *L)
{
	LuaParams p(L);

	CMenu *menu = GPPGame::GuitarPP().getActualMenu();

	if (menu)
	{
		p << menu->getName();
	}
	else
	{
		p << "";
	}

	return p.rtn();
}

/*
*
*/
int CLuaFunctions::getGameCallback(lua_State *L)
{
	LuaParams p(L);

	if (p.getNumParams() == 1 && lua_isstring(L, 1))
	{
		std::string param = lua_tostring(L, 1);

		for (auto &s : GPPGame::GuitarPP().gameCallbacksWrapper)
		{
			if (s.second == param)
			{
				p << s.first;
				break;
			}
		}
	}

	return p.rtn();
}

void CLuaFunctions::registerFunctions(lua_State *L)
{
	lua_register(L, "setConfigs", setConfigs);
	lua_register(L, "doNotRunAgain", doNotRunAgain);
	lua_register(L, "setCallBackToEvent", setCallBackToEvent);
	lua_register(L, "drawTextOnScreen", drawTextOnScreen);
	lua_register(L, "newMenuOption", newMenuOption);
	lua_register(L, "newMenu", newMenu);
	lua_register(L, "getMainMenuName", getMainMenuName);
	lua_register(L, "printTolog", printTolog);
	lua_register(L, "getMenuOptionName", getMenuOptionName);
	lua_register(L, "getNumOfMenuOptions", getNumOfMenuOptions);
	lua_register(L, "loadTexture", loadTexture);
	lua_register(L, "addTextureToFont", addTextureToFont);
	lua_register(L, "assingMenuToOtherMenuOption", assingMenuToOtherMenuOption);
	lua_register(L, "setMenuBackgroundTexture", setMenuBackgroundTexture);
	lua_register(L, "getActualMenu", getActualMenu);
	lua_register(L, "getGameCallback", getGameCallback);
	lua_register(L, "assingGameFunctionToMenuOption", assingGameFunctionToMenuOption);
}


template<class T> void setLuaGlobal(lua_State *L, const std::string &name, const T &value)
{
	CLuaH::customParam(value).pushToLuaStack(L);
	lua_setglobal(L, name.c_str());
}

/*
* Register default game globals
*/
void CLuaFunctions::registerGlobals(lua_State *L)
{
	setLuaGlobal(L, "TESTE", "abcde");
	setLuaGlobal(L, "VSYNC_OFF", 0);
	setLuaGlobal(L, "VSYNC_ON", 1);
	setLuaGlobal(L, "VSYNC_HALF", 2);
	setLuaGlobal(L, "VSYNC_AUTO", 3);


}

CLuaFunctions::CLuaFunctions()
{

}


