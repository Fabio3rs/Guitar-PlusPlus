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
		param = (bool)lua_toboolean(L, stck);
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

int CLuaFunctions::setConfigs(lua_State *L){
	auto cfg = GPPGame::GuitarPP().getWindowDefaults();


	lua_getglobal(L, "WindowWidth");

	if (lua_isnumber(L, -1))
		cfg.w = lua_tointeger(L, -1);



	lua_getglobal(L, "WindowHeight");

	if (lua_isnumber(L, -1))
		cfg.h = lua_tointeger(L, -1);



	lua_getglobal(L, "FullScreen");

	if (lua_isboolean(L, -1))
		cfg.fullscreen = (bool)lua_toboolean(L, -1);



	lua_getglobal(L, "customTitle");

	if (lua_isstring(L, -1))
		cfg.name = lua_tostring(L, -1);


	GPPGame::GuitarPP().settWindowConfigs(cfg);

	return 0;
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
		CFonts::fonts().DrawTextInGLFWWindow(lua_tostring(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4));
	}
	return p.rtn();
}

/*
* New menu
*/
int CLuaFunctions::newMenu(lua_State *L){
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
* Output a string in game log
*/
int CLuaFunctions::printTolog(lua_State *L){
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
int CLuaFunctions::newMenuOption(lua_State *L){
	LuaParams p(L);

	if (p.getNumParams() == 9 && lua_isstring(L, 1) && lua_isstring(L, 2) && lua_isnumber(L, 3) && lua_isnumber(L, 4) && lua_isnumber(L, 5)
		&& lua_isnumber(L, 6) && lua_isnumber(L, 7) && lua_isnumber(L, 8) && lua_isfunction(L, 9)){
		std::string menuName;
		int optID = 0;

		p >> menuName;

		CMenu::menuOpt opt;

		p >> opt.text;
		p >> opt.x;
		p >> opt.y;
		p >> opt.size;
		p >> opt.group;
		p >> opt.status;
		p >> opt.type;

		p << (optID = GPPGame::GuitarPP().getMenuByName(menuName).addOpt(opt));

		// ************************************************************** auto callback system
		lua_pushvalue(L, 9);
		int	fnRef = luaL_ref(L, LUA_REGISTRYINDEX);

		// event menuOptionName
		CLuaH::Lua().getLastScript().callbacks[GPPGame::GuitarPP().getMenuByName(menuName).getNameFromOpt(optID)] = fnRef;
	}
	return p.rtn();
}

/*
*
*/
int CLuaFunctions::getMenuOptionName(lua_State *L){
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
int CLuaFunctions::getNumOfMenuOptions(lua_State *L){
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
int CLuaFunctions::getMainMenuName(lua_State *L){
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
}

CLuaFunctions::CLuaFunctions()
{

}


