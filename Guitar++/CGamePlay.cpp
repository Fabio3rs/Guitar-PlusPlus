#include "CGamePlay.h"
#include "CEngine.h"
#include "GPPGame.h"

void CGamePlay::updatePlayer(CPlayer &player)
{




}

void CGamePlay::update()
{
	for (auto &p : players)
	{
		updatePlayer(p);
	}
}

void CGamePlay::resetModule()
{
	players.clear();

}

void CGamePlay::render()
{
	CEngine::RenderDoubleStruct RenderData;

	auto &game = GPPGame::GuitarPP();
	auto &menu = game.loadTexture("data/sprites", "83sfq98a.tga");

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

	CEngine::engine().Render2DQuad(RenderData);




}

/*
CGamePlay &CGamePlay::gamePlay()
{
	static CGamePlay game;
	return game;
}*/

CGamePlay::CGamePlay()
{


}



