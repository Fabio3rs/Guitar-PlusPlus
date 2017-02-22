#include "CCampaing.h"

CCampaing::CCampaingData::CCampaingData()
{
	money = 0.0;
	reputationPoints = 0;

}

int CCampaing::getCampaingList(lua_State *L)
{
	CLuaFunctions::LuaParams p(L);

	p << "TODO";

	return p.rtn();
}

int CCampaing::registerLuaFunctions(lua_State *L)
{
	lua_register(L, "getCampaingList", getCampaingList);

	return 0;
}

CCampaing &CCampaing::campaingMGR()
{
	static CCampaing campaing;
	return campaing;
}

CCampaing::CCampaing()
{
	CLuaFunctions::LuaF().registerLuaFuncsAPI(registerLuaFunctions);
}
