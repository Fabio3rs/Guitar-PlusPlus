#include "CLuaFunctions.hpp"
#include "GPPGame.h"
#include "CFonts.h"
#include <iostream>
#include "CLog.h"

void CLuaFunctions::registerLuaFuncsAPI(std::function<int(CLuaH::luaState &)> fun)
{
	registerFunctionsAPICBs.push_back(fun);
}

void CLuaFunctions::registerLuaGlobalsAPI(std::function<int(CLuaH::luaState &)> fun)
{
	registerGlobalsAPICBs.push_back(fun);
}

void CLuaFunctions::registerFrameUpdateAPI(std::function<void(void)> fun)
{
	frameUpdateAPICBs.push_back(fun);
}

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

CLuaFunctions::LuaParams &CLuaFunctions::LuaParams::operator<<(int64_t param)
{
	lua_pushinteger(L, param);
	++ret;

	return *this;
}

CLuaFunctions::LuaParams &CLuaFunctions::LuaParams::operator<<(size_t param)
{
	lua_pushinteger(L, param);
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
	if (stck <= num_params)
	{
		param = static_cast<int>(lua_tointeger(L, stck));
		++stck;
	}
	else
	{
		fail_bit = 1;
	}

	return *this;
}

CLuaFunctions::LuaParams &CLuaFunctions::LuaParams::operator>>(int64_t &param)
{
	if (stck <= num_params)
	{
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
		param = (void*)lua_topointer(L, stck);
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

CLuaFunctions::GameVariables &CLuaFunctions::GameVariables::gv()
{
	static GameVariables gvarsmgr;
	return gvarsmgr;
}

void CLuaFunctions::GameVariables::pushVar(const std::string &name, int &var)
{
	vard v;
	v.ptr = &var;
	v.t = integerv;

	vars[name] = v;
}

void CLuaFunctions::GameVariables::pushVar(const std::string &name, int64_t &var)
{
	vard v;
	v.ptr = &var;
	v.t = integer64;

	vars[name] = v;
}

void CLuaFunctions::GameVariables::pushVar(const std::string &name, double &var)
{
	vard v;
	v.ptr = &var;
	v.t = doublevar;

	vars[name] = v;
}

void CLuaFunctions::GameVariables::pushVar(const std::string &name, std::string &var)
{
	vard v;
	v.ptr = &var;
	v.t = stringvar;

	vars[name] = v;
}

void CLuaFunctions::GameVariables::pushVar(const std::string &name, bool &var)
{
	vard v;
	v.ptr = &var;
	v.t = booleanvar;

	vars[name] = v;
}

void CLuaFunctions::GameVariables::setVar(const std::string &name, int64_t value)
{
	auto &v = vars[name];

	switch (v.t)
	{
	case integerv:
		*(int*)v.ptr = static_cast<int>(value);
		break;

	case integer64:
		*(int64_t*)v.ptr = value;
		break;

	case doublevar:
		*(double*)v.ptr = static_cast<double>(value);
		break;

	case booleanvar:
		*(bool*)v.ptr = value != 0;
		break;

	case stringvar:
		*(std::string*)v.ptr = std::to_string(value);
		break;
	}
}

void CLuaFunctions::GameVariables::setVar(const std::string &name, double value)
{
	auto &v = vars[name];

	switch (v.t)
	{
	case integerv:
		*(int*)v.ptr = static_cast<int>(value);
		break;

	case integer64:
		*(int64_t*)v.ptr = static_cast<int64_t>(value);
		break;

	case doublevar:
		*(double*)v.ptr = value;
		break;

	case booleanvar:
		*(bool*)v.ptr = value != 0;
		break;

	case stringvar:
		*(std::string*)v.ptr = std::to_string(value);
		break;
	}
}

void CLuaFunctions::GameVariables::setVar(const std::string &name, std::string value)
{
	auto &v = vars[name];

	switch (v.t)
	{
	case integerv:
		*(int*)v.ptr = std::stoi(value);
		break;

	case integer64:
		*(int64_t*)v.ptr = std::stoll(value);
		break;

	case doublevar:
		*(double*)v.ptr = std::stod(value);
		break;

	case booleanvar:
		*(bool*)v.ptr = std::stoi(value) != 0;
		break;

	case stringvar:
		*(std::string*)v.ptr = value;
		break;
	}
}

void CLuaFunctions::GameVariables::setVar(const std::string &name, bool value)
{
	auto &v = vars[name];

	switch (v.t)
	{
	case integerv:
		*(int*)v.ptr = value;
		break;

	case integer64:
		*(int64_t*)v.ptr = value;
		break;

	case doublevar:
		*(double*)v.ptr = value;
		break;

	case booleanvar:
		*(bool*)v.ptr = value;
		break;

	case stringvar:
		*(std::string*)v.ptr = std::to_string(value);
		break;
	}
}

void CLuaFunctions::GameVariables::setVar(const std::string &name, int value)
{
	auto &v = vars[name];

	switch (v.t)
	{
	case integerv:
		*(int*)v.ptr = value;
		break;

	case integer64:
		*(int64_t*)v.ptr = value;
		break;

	case doublevar:
		*(double*)v.ptr = value;
		break;

	case booleanvar:
		*(bool*)v.ptr = value;
		break;

	case stringvar:
		*(std::string*)v.ptr = std::to_string(value);
		break;
	}
}

void CLuaFunctions::GameVariables::setVar(const std::string &name, lua_State *L, int stackIDX)
{
	CLuaH::customParam c;

	c.getFromArgs(L, stackIDX);

	auto &v = vars[name];

	switch (v.t)
	{
	case integerv:
		setVar(name, c.getNumber());
		break;

	case doublevar:
		setVar(name, c.getNumber());
		break;

	case booleanvar:
		setVar(name, c.getBoolean());
		break;

	case stringvar:
		setVar(name, c.getString());
		break;
	}
}

void CLuaFunctions::GameVariables::pushToLuaStack(const std::string &name, lua_State *L)
{
	auto &v = vars[name];
	CLuaH::customParam c;

	switch (v.t)
	{
	case integerv:
		c = CLuaH::customParam(*(int*)v.ptr);
		c.pushToLuaStack(L);
		break;

	case integer64:
		c = CLuaH::customParam(*(int64_t*)v.ptr);
		c.pushToLuaStack(L);
		break;

	case doublevar:
		c = CLuaH::customParam(*(double*)v.ptr);
		c.pushToLuaStack(L);
		break;

	case booleanvar:
		c = CLuaH::customParam(*(bool*)v.ptr);
		c.pushToLuaStack(L);
		break;

	case stringvar:
		c = CLuaH::customParam(*(std::string*)v.ptr);
		c.pushToLuaStack(L);
		break;
	}
}

CLuaFunctions::GameVariables::GameVariables()
{

}

int CLuaFunctions::setGameVar(lua_State *L)
{
	LuaParams p(L);

	if (p.getNumParams() == 2 && lua_isstring(L, 1))
	{
		std::string varname;

		p >> varname;

		GameVariables::gv().setVar(varname, L, 2);
	}

	return p.rtn();
}

int CLuaFunctions::getGameVar(lua_State *L)
{
	LuaParams p(L);

	if (p.getNumParams() == 1 && lua_isstring(L, 1))
	{
		std::string varname;

		p >> varname;

		GameVariables::gv().pushToLuaStack(varname, L);
	}

	return p.rtn();
}

int CLuaFunctions::printGlobals(lua_State *L)
{
	CLuaH::customParam c;
	CLuaH::loadGlobalTable(L, c);
	return 0;
}

int CLuaFunctions::setConfigs(lua_State *L)
{
	auto cfg = GPPGame::GuitarPP().getWindowConfig();


	lua_getglobal(L, "WindowWidth");

	if (lua_isnumber(L, -1))
		cfg.w = static_cast<int>(lua_tointeger(L, -1));



	lua_getglobal(L, "WindowHeight");

	if (lua_isnumber(L, -1))
		cfg.h = static_cast<int>(lua_tointeger(L, -1));



	lua_getglobal(L, "FullScreen");

	if (lua_isboolean(L, -1)){
		cfg.fullscreen = lua_toboolean(L, -1) != 0;
	}

	lua_getglobal(L, "Borderless");

	if (lua_isboolean(L, -1))
		cfg.fullscreen |= (lua_toboolean(L, -1) != 0) ? 2 : 0;

	lua_getglobal(L, "customTitle");

	if (lua_isstring(L, -1))
		cfg.name = lua_tostring(L, -1);



	lua_getglobal(L, "VSync");

	if (lua_isnumber(L, -1))
		cfg.VSyncMode = static_cast<int>(lua_tointeger(L, -1));



	lua_getglobal(L, "AntiAliasing");

	if (lua_isnumber(L, -1))
		cfg.AA = static_cast<int>(lua_tointeger(L, -1));



	lua_getglobal(L, "Green");

	if (lua_isnumber(L, -1))
		GPPGame::GuitarPP().strumKeys[0] = static_cast<unsigned int>(lua_tointeger(L, -1));


	lua_getglobal(L, "Red");

	if (lua_isnumber(L, -1))
		GPPGame::GuitarPP().strumKeys[1] = static_cast<unsigned int>(lua_tointeger(L, -1));


	lua_getglobal(L, "Yellow");

	if (lua_isnumber(L, -1))
		GPPGame::GuitarPP().strumKeys[2] = static_cast<unsigned int>(lua_tointeger(L, -1));


	lua_getglobal(L, "Blue");

	if (lua_isnumber(L, -1))
		GPPGame::GuitarPP().strumKeys[3] = static_cast<unsigned int>(lua_tointeger(L, -1));


	lua_getglobal(L, "Orange");

	if (lua_isnumber(L, -1))
		GPPGame::GuitarPP().strumKeys[4] = static_cast<unsigned int>(lua_tointeger(L, -1));


	lua_getglobal(L, "fretOne");

	if (lua_isnumber(L, -1))
		GPPGame::GuitarPP().fretOneKey = static_cast<unsigned int>(lua_tointeger(L, -1));


	lua_getglobal(L, "fretTwo");

	if (lua_isnumber(L, -1))
		GPPGame::GuitarPP().fretTwoKey = static_cast<unsigned int>(lua_tointeger(L, -1));



	lua_getglobal(L, "Song");

	if (lua_isstring(L, -1))
		GPPGame::GuitarPP().songToLoad = lua_tostring(L, -1);


	lua_getglobal(L, "hyperSpeed");

	if (lua_isnumber(L, -1))
		GPPGame::GuitarPP().hyperSpeed = lua_tonumber(L, -1);



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

int CLuaFunctions::loadSound(lua_State * L)
{
	LuaParams p(L);

	for (int i = 0; i < p.getNumParams(); i++) {
		if (lua_isstring(L, i + 1)) {
			std::string str;
			p >> str;

			int result = 0;

			if (CEngine::engine().loadSoundStream(str.c_str(), result))
			{
				p << result;
			}
			else {
				p << false;
			}
		}
		else {
			p << false;
		}
	}

	return p.rtn();
}

int CLuaFunctions::loadSoundDecode(lua_State * L)
{
	LuaParams p(L);

	for (int i = 0; i < p.getNumParams(); i++) {
		if (lua_isstring(L, i + 1)) {
			std::string str = "<<nullstring>>";
			p >> str;

			int result = 0;

			if (CEngine::engine().loadSoundStream(str.c_str(), result, true))
			{
				p << result;
			}
			else {
				p << false;
			}
		}
		else {
			p << false;
		}
	}

	return p.rtn();
}

int CLuaFunctions::playSound(lua_State * L)
{
	LuaParams p(L);

	for (int i = 0; i < p.getNumParams(); i++) {
		if (lua_isnumber(L, i + 1)) {
			int handle = 0;
			p >> handle;

			p << CEngine::engine().playSoundStream(handle);
		}
		else
		{
			p << false;
		}
	}

	return p.rtn();
}

int CLuaFunctions::pauseSound(lua_State * L)
{
	LuaParams p(L);

	for (int i = 0; i < p.getNumParams(); i++) {
		if (lua_isnumber(L, i + 1)) {
			int handle = 0;
			p >> handle;

			p << CEngine::engine().pauseSoundStream(handle);
		}
		else
		{
			p << false;
		}
	}

	return p.rtn();
}

int CLuaFunctions::stopSound(lua_State * L)
{
	LuaParams p(L);

	for (int i = 0; i < p.getNumParams(); i++) {
		if (lua_isnumber(L, i + 1)) {
			int handle = 0;
			p >> handle;


			if (CEngine::engine().pauseSoundStream(handle))
			{
				p << true;
				CEngine::engine().setSoundTime(handle, 0.0);
			}
			else
			{
				p << false;
			}
		}
		else
		{
			p << false;
		}
	}

	return p.rtn();
}

int CLuaFunctions::setSoundTime(lua_State * L)
{
	LuaParams p(L);

	if (p.getNumParams() == 2 && lua_isnumber(L, 1) && lua_isnumber(L, 2))
	{
		int handle = 0;
		double value = 0.0;

		p >> handle;
		p >> value;

		CEngine::engine().setSoundTime(handle, value);
	}

	return p.rtn();
}

int CLuaFunctions::setSoundVolume(lua_State * L)
{
	LuaParams p(L);

	if (p.getNumParams() == 2 && lua_isnumber(L, 1) && lua_isnumber(L, 2))
	{
		int handle = 0;
		double value = 0.0;

		p >> handle;
		p >> value;

		p << CEngine::engine().setSoundVolume(handle, static_cast<float>(value));
	}

	return p.rtn();
}

int CLuaFunctions::getSoundVolume(lua_State * L)
{
	LuaParams p(L);

	for (int i = 0; i < p.getNumParams(); i++) {
		if (lua_isnumber(L, i + 1)) {
			int handle = 0;
			p >> handle;

			p << CEngine::engine().getSoundVolume(handle);
		}
		else
		{
			p << false;
		}
	}

	return p.rtn();
}

int CLuaFunctions::getSoundTime(lua_State * L)
{
	LuaParams p(L);

	for (int i = 0; i < p.getNumParams(); i++) {
		if (lua_isnumber(L, i + 1)) {
			int handle = 0;
			p >> handle;

			p << CEngine::engine().getSoundTime(handle);
		}
		else
		{
			p << false;
		}
	}

	return p.rtn();
}

int CLuaFunctions::releaseSound(lua_State * L)
{
	LuaParams p(L);

	for (int i = 0; i < p.getNumParams(); i++) {
		if (lua_isnumber (L, i + 1)) {
			int handle = 0;
			p >> handle;

			p << CEngine::engine().unloadSoundStream(handle);
		}
		else
		{
			p << false;
		}
	}

	return p.rtn();
}

int CLuaFunctions::setSoundFlags(lua_State * L)
{
	LuaParams p(L);

	if (p.getNumParams() == 3 && lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_isnumber(L, 3))
	{
		int handle = 0;
		int64_t flags = 0;
		int64_t mask = 0;

		p >> handle;
		p >> flags;
		p >> mask;

		p << CEngine::engine().setSoundFlags(handle, static_cast<int>(flags), static_cast<int>(mask));
	}

	return p.rtn();
}

int CLuaFunctions::setSoundAttribute(lua_State * L)
{
	LuaParams p(L);

	if (p.getNumParams() == 3 && lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_isnumber(L, 3))
	{
		int handle = 0;
		int flags = 0;
		double value = 0;

		p >> handle;
		p >> flags;
		p >> value;

		p << CEngine::engine().setSoundAttribute(handle, flags, static_cast<float>(value));
	}

	return p.rtn();
}

int CLuaFunctions::getBassError(lua_State * L)
{
	LuaParams p(L);

	p << CEngine::getBassError();

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

			result = GPPGame::GuitarPP().loadTexture("data/sprites", str, &CLuaH::Lua().getLuaStateScript(L)).getTextId();

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
	CLuaH::Lua().getLuaStateScript(L).runAgain = false;

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

		CLuaH::Lua().getLuaStateScript(L).callbacks[eventName] = fnRef;
		CLuaH::Lua().getLuaStateScript(L).callbacksAdded = true;
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

		if (opt.type == CMenu::menusOPT::none)
		{
			opt.type = CMenu::menusOPT::textbtn;
			opt.goback = true;
		}

		p << (optID = GPPGame::GuitarPP().getMenuByName(menuName).addOpt(opt));

		// ************************************************************** auto callback system
		lua_pushvalue(L, 8);
		int	fnRef = luaL_ref(L, LUA_REGISTRYINDEX);

		// event menuOptionName
		CLuaH::Lua().getLuaStateScript(L).callbacks[GPPGame::GuitarPP().getMenuByName(menuName).getNameFromOpt(optID)] = fnRef;
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

void CLuaFunctions::registerFunctions(CLuaH::luaState &Lstate)
{
	lua_State *L = Lstate.get();

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
	lua_register(L, "setGameVar", setGameVar);

	lua_register(L, "loadSound", loadSound);
	lua_register(L, "loadSoundDecode", loadSoundDecode);
	lua_register(L, "playSound", playSound);
	lua_register(L, "stopSound", stopSound);
	lua_register(L, "setSoundTime", setSoundTime);
	lua_register(L, "setSoundVolume", setSoundVolume);
	lua_register(L, "releaseSound", releaseSound);
	lua_register(L, "setSoundFlags", setSoundFlags);
	lua_register(L, "getSoundTime", getSoundTime);
	lua_register(L, "getSoundVolume", getSoundVolume);
	lua_register(L, "setSoundAttribute", setSoundAttribute);
	lua_register(L, "getBassError", getBassError);
	lua_register(L, "printGlobals", printGlobals);

	auto &funList = LuaF().registerFunctionsAPICBs;

	for (auto &fun : funList)
	{
		fun(Lstate);
	}
}


template<class T> void setLuaGlobal(lua_State *L, const std::string &name, const T &value)
{
	CLuaH::customParam(value).pushToLuaStack(L);
	lua_setglobal(L.get(), name.c_str());
}

template<class T> void setLuaGlobal(CLuaH::luaState &L, const std::string &name, const T &value)
{
	CLuaH::customParam(value).pushToLuaStack(L);
	lua_setglobal(L.get(), name.c_str());
}

#include <bass.h>
#include <bass_fx.h>

/*
* Register default game globals
*/
void CLuaFunctions::registerGlobals(CLuaH::luaState &L)
{
	setLuaGlobal(L, "TESTE", "abcde");
	setLuaGlobal(L, "VSYNC_OFF", 0);
	setLuaGlobal(L, "VSYNC_ON", 1);
	setLuaGlobal(L, "VSYNC_HALF", 2);
	setLuaGlobal(L, "VSYNC_AUTO", 3);

	setLuaGlobal(L, "BASS_SAMPLE_LOOP", BASS_SAMPLE_LOOP);
	setLuaGlobal(L, "BASS_STREAM_AUTOFREE", BASS_STREAM_AUTOFREE);
	setLuaGlobal(L, "BASS_STREAM_RESTRATE", BASS_STREAM_RESTRATE);
	setLuaGlobal(L, "BASS_MUSIC_NONINTER", BASS_MUSIC_NONINTER);
	setLuaGlobal(L, "BASS_MUSIC_SINCINTER", BASS_MUSIC_SINCINTER);
	setLuaGlobal(L, "BASS_MUSIC_SURROUND", BASS_MUSIC_SURROUND);
	setLuaGlobal(L, "BASS_MUSIC_SURROUND2", BASS_MUSIC_SURROUND2);
	setLuaGlobal(L, "BASS_MUSIC_FT2MOD", BASS_MUSIC_FT2MOD);
	setLuaGlobal(L, "BASS_MUSIC_PT1MOD", BASS_MUSIC_PT1MOD);
	setLuaGlobal(L, "BASS_MUSIC_POSRESET", BASS_MUSIC_POSRESET);
	setLuaGlobal(L, "BASS_MUSIC_POSRESETEX", BASS_MUSIC_POSRESETEX);
	setLuaGlobal(L, "BASS_MUSIC_STOPBACK", BASS_MUSIC_STOPBACK);


	setLuaGlobal(L, "BASS_ATTRIB_EAXMIX", BASS_ATTRIB_EAXMIX);
	setLuaGlobal(L, "BASS_ATTRIB_CPU", BASS_ATTRIB_CPU);
	setLuaGlobal(L, "BASS_ATTRIB_FREQ", BASS_ATTRIB_FREQ);
	setLuaGlobal(L, "BASS_ATTRIB_MUSIC_ACTIVE", /*BASS_ATTRIB_MUSIC_ACTIVE*/-1);
	setLuaGlobal(L, "BASS_ATTRIB_MUSIC_AMPLIFY", BASS_ATTRIB_MUSIC_AMPLIFY);
	setLuaGlobal(L, "BASS_ATTRIB_MUSIC_BPM", BASS_ATTRIB_MUSIC_BPM);
	setLuaGlobal(L, "BASS_ATTRIB_MUSIC_PANSEP", BASS_ATTRIB_MUSIC_PANSEP);
	setLuaGlobal(L, "BASS_ATTRIB_MUSIC_PSCALER", BASS_ATTRIB_MUSIC_PSCALER);
	setLuaGlobal(L, "BASS_ATTRIB_MUSIC_SPEED", BASS_ATTRIB_MUSIC_SPEED);
	setLuaGlobal(L, "BASS_ATTRIB_MUSIC_VOL_CHAN", BASS_ATTRIB_MUSIC_VOL_CHAN);
	setLuaGlobal(L, "BASS_ATTRIB_MUSIC_VOL_GLOBAL", BASS_ATTRIB_MUSIC_VOL_GLOBAL);
	setLuaGlobal(L, "BASS_ATTRIB_MUSIC_VOL_INST", BASS_ATTRIB_MUSIC_VOL_INST);
	setLuaGlobal(L, "BASS_ATTRIB_NOBUFFER", BASS_ATTRIB_NOBUFFER);
	setLuaGlobal(L, "BASS_ATTRIB_PAN", BASS_ATTRIB_PAN);
	setLuaGlobal(L, "BASS_ATTRIB_SRC", BASS_ATTRIB_SRC);
	setLuaGlobal(L, "BASS_ATTRIB_VOL", BASS_ATTRIB_VOL);
	setLuaGlobal(L, "BASS_ATTRIB_TEMPO", BASS_ATTRIB_TEMPO);
	setLuaGlobal(L, "BASS_ATTRIB_TEMPO_PITCH", BASS_ATTRIB_TEMPO_PITCH);
	setLuaGlobal(L, "BASS_ATTRIB_TEMPO_FREQ", BASS_ATTRIB_TEMPO_FREQ);
	setLuaGlobal(L, "BASS_ATTRIB_TEMPO_OPTION_USE_AA_FILTER", BASS_ATTRIB_TEMPO_OPTION_USE_AA_FILTER);
	setLuaGlobal(L, "BASS_ATTRIB_TEMPO_OPTION_AA_FILTER_LENGTH", BASS_ATTRIB_TEMPO_OPTION_AA_FILTER_LENGTH);
	setLuaGlobal(L, "BASS_ATTRIB_TEMPO_OPTION_USE_QUICKALGO", BASS_ATTRIB_TEMPO_OPTION_USE_QUICKALGO);
	setLuaGlobal(L, "BASS_ATTRIB_TEMPO_OPTION_SEQUENCE_MS", BASS_ATTRIB_TEMPO_OPTION_SEQUENCE_MS);
	setLuaGlobal(L, "BASS_ATTRIB_TEMPO_OPTION_USE_QUICKALGO", BASS_ATTRIB_TEMPO_OPTION_USE_QUICKALGO);
	setLuaGlobal(L, "BASS_ATTRIB_TEMPO_OPTION_SEEKWINDOW_MS", BASS_ATTRIB_TEMPO_OPTION_SEEKWINDOW_MS);
	setLuaGlobal(L, "BASS_ATTRIB_TEMPO_OPTION_OVERLAP_MS", BASS_ATTRIB_TEMPO_OPTION_OVERLAP_MS);
	setLuaGlobal(L, "BASS_ATTRIB_REVERSE_DIR", BASS_ATTRIB_REVERSE_DIR);

	auto &funList = LuaF().registerGlobalsAPICBs;

	for (auto &fun : funList)
	{
		fun(L);
	}
}

CLuaFunctions::CLuaFunctions()
{

}


