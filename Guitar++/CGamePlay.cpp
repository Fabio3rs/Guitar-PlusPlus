#include "CGamePlay.h"
#include "CEngine.h"
#include "GPPGame.h"
#include "CFonts.h"


void CGamePlay::renderIndivdualStrikeButton(int id, double pos, unsigned int Texture, int state, CPlayer &player) {
	CEngine::RenderDoubleStruct TempStruct3D;

	//auto &frets = GPPGame::GuitarPP().frets[fretsTextures];

	double nCalc = pos;

	const double size = 0.25;
	const double position = -0.56;

	TempStruct3D.Text = Texture;

	TempStruct3D.TextureX1 = fretsText.FretIMGPos[fretsText.sAIDTI(state, id)].x * fretsText.columnSize;
	TempStruct3D.TextureX2 = TempStruct3D.TextureX1 + fretsText.columnSize;
	TempStruct3D.TextureY1 = 1.0 - fretsText.FretIMGPos[fretsText.sAIDTI(state, id)].y * fretsText.lineFretSize;
	TempStruct3D.TextureY2 = TempStruct3D.TextureY1 - fretsText.lineFretSize;

	TempStruct3D.x1 = position + (double(id) * size / 1.15);
	TempStruct3D.x2 = TempStruct3D.x1 + size;
	TempStruct3D.x3 = TempStruct3D.x1 + size;
	TempStruct3D.x4 = TempStruct3D.x1;

	TempStruct3D.y1 = -0.5;
	TempStruct3D.y2 = TempStruct3D.y1;
	TempStruct3D.y3 = -0.5;
	TempStruct3D.y4 = TempStruct3D.y3;

	TempStruct3D.z1 = nCalc;
	TempStruct3D.z2 = TempStruct3D.z1;
	TempStruct3D.z3 = nCalc + size * 4.0;
	TempStruct3D.z4 = TempStruct3D.z3;

	CEngine::engine().Render3DQuad(TempStruct3D);
}


double CGamePlay::getRunningMusicTime(CPlayer &player)
{
	return CEngine::engine().getTime() - player.startTime;;
}

double CGamePlay::time2Position(double Time, CPlayer &player){
	return ((getRunningMusicTime(player) - Time) * -1.0) / 1.5;
}

double CGamePlay::pos2Alpha(double pos){
	double result = 0.0;

	double minPosMinAlpha = 1.5, maxPosMaxAlpha = 0.6, dif = minPosMinAlpha - maxPosMaxAlpha;

	if (pos > minPosMinAlpha){
		return 0.0;
	}

	if (pos < maxPosMaxAlpha){
		return 1.0;
	}

	result = (1.0 / dif) * (minPosMinAlpha - pos);

	return result;
}

void CGamePlay::renderIndivdualNote(int id, double pos, unsigned int Texture, CPlayer &player){
	CEngine::RenderDoubleStruct TempStruct3D;
	double rtime = getRunningMusicTime(player) - pos;

	if (rtime > -5.0){
		double size = 0.2;
		double position = -0.51;

		TempStruct3D.Text = Texture;
		TempStruct3D.TextureX1 = double(id) * 0.2;
		TempStruct3D.TextureX2 = double(id) * 0.2 + 0.2;

		double nCalc = rtime * speedMp;

		nCalc += 0.55;

		if (player.plusEnabled){
			TempStruct3D.TextureY1 = 0.5;
			TempStruct3D.TextureY2 = 0.0;
		}
		else{
			TempStruct3D.TextureY1 = 1.0;
			TempStruct3D.TextureY2 = 0.5;
		}

		TempStruct3D.x1 = position + (double(id) * size / 48.0) + (double(id) * size);
		TempStruct3D.x2 = TempStruct3D.x1 + size;
		TempStruct3D.x3 = TempStruct3D.x1 + size;
		TempStruct3D.x4 = TempStruct3D.x1;

		TempStruct3D.y1 = -0.5;
		TempStruct3D.y2 = TempStruct3D.y1;
		TempStruct3D.y3 = -0.5;
		TempStruct3D.y4 = TempStruct3D.y3;

		TempStruct3D.z1 = nCalc;
		TempStruct3D.z2 = TempStruct3D.z1;
		TempStruct3D.z3 = nCalc + size * 2.0;
		TempStruct3D.z4 = TempStruct3D.z3;

		CEngine::engine().setColor(1.0, 1.0, 1.0, pos2Alpha(- TempStruct3D.z1 / 5.5));

		CEngine::engine().Render3DQuad(TempStruct3D);
	}
}

void CGamePlay::renderIndividualLine(int id, double pos1, double pos2, unsigned int Texture, CPlayer &player)
{
	CEngine::RenderDoubleStruct TempStruct3D;

	double runningTime = getRunningMusicTime(player);
	double rtime = runningTime - pos1;
	double rtime2 = runningTime - (pos1 + pos2);

	const double size = 0.2;
	const double position = -0.51;

	double nCalc = rtime * speedMp;
	double nCalc2 = rtime2 * speedMp;

	nCalc += 0.55 + size;
	nCalc2 += 0.55 + size * 1.5;

	TempStruct3D.Text = Texture;
	TempStruct3D.TextureX1 = 0.0;
	TempStruct3D.TextureX2 = 1.0;
	TempStruct3D.TextureY1 = 1.0;
	TempStruct3D.TextureY2 = 0.0;

	TempStruct3D.x1 = position + (double(id) * size / 48.0) + (double(id) * size);
	TempStruct3D.x2 = TempStruct3D.x1 + size;
	TempStruct3D.x3 = TempStruct3D.x1 + size;
	TempStruct3D.x4 = TempStruct3D.x1;

	TempStruct3D.y1 = -0.5;
	TempStruct3D.y2 = TempStruct3D.y1;
	TempStruct3D.y3 = -0.5;
	TempStruct3D.y4 = TempStruct3D.y3;

	TempStruct3D.z1 = nCalc;
	TempStruct3D.z2 = TempStruct3D.z1;
	TempStruct3D.z3 = nCalc2;
	TempStruct3D.z4 = TempStruct3D.z3;

	//CEngine::engine().setColor(1.0, 1.0, 1.0, 1.0);

	CEngine::engine().Render3DQuad(TempStruct3D);
}

void CGamePlay::renderNote(CPlayer::NotesData::Note &note, CPlayer &player){
	double time = /*time2Position(*/note.time/*)*/;
	for (int i = 0; i < 5; i++){
		if (note.type & (int)pow(2, i)){
			unsigned int texture = fretsText.notesTexture;
			if (note.type & notesFlags::nf_doing_slide){
				time = 0.0;
			}

			if (note.lTime > 0.0 && note.type & notesFlags::nf_doing_slide){
				renderIndividualLine(i, time, note.lTime, GPPGame::GuitarPP().loadTexture("data/sprites", "line.tga").getTextId(), player);
			}
			else if (note.type & notesFlags::nf_slide && !(note.type & notesFlags::nf_slide_picked)){
				renderIndividualLine(i, time, note.lTime, GPPGame::GuitarPP().loadTexture("data/sprites", "line.tga").getTextId(), player);
			}

			/*if ((note.type & notesFlags::nf_not_hopo) ^ notesFlags::nf_not_hopo){
				texture = CTheme::inst().SPR["HopoNote"];
			}*/

			/*if (note.type & notesFlags::plus_mid || note.type & notesFlags::plus_end){
			texture = CTheme::inst().SPR["PlusNote"];
			}*/

			if (!(note.type & notesFlags::nf_picked)) renderIndivdualNote(i, time, texture, player);
		}
	}
}

void CGamePlay::updatePlayer(CPlayer &player)
{
	auto &notes = player.Notes;
	auto &gNotes = player.Notes.gNotes;
	auto &engine = CEngine::engine();

	player.buffer.clear();

	double musicTime = engine.getTime() - player.startTime;

	for (size_t i = notes.notePos, size = gNotes.size(); i < size; i++)
	{
		auto &note = gNotes[i];
		double noteTime = note.time - musicTime;
		double endNoteTime = noteTime + note.lTime;

		if (endNoteTime > -1.5 && noteTime < 5.0)
		{
			player.buffer.push_front(note);
		}
		else if (endNoteTime < -1.5)
		{
			notes.notePos = i;
		}
		else if (endNoteTime > 5)
		{
			break;
		}
	}


}

void CGamePlay::renderFretBoard(CPlayer &player, double x1, double x2, double x3, double x4, unsigned int Text){
	auto positionCalcByT = [this](double p, double prop){
		double cCalc = -p * 5.0;
		double propSpeeed = 5.0 / speedMp;

		cCalc /= propSpeeed;

		cCalc *= 100000.0;
		cCalc = (int)cCalc % (int)(150000 * prop);
		return cCalc /= 100000.0;
	};
	
	double musicRunningTime = getRunningMusicTime(player) - 4.0;
	double cCalc = positionCalcByT(musicRunningTime, (x2 - x1));

	for (int i = -2; i < 8; i++){
		CEngine::RenderDoubleStruct FretBoardStruct;

		FretBoardStruct.x1 = x1;
		FretBoardStruct.x2 = x2;
		FretBoardStruct.x3 = x3;
		FretBoardStruct.x4 = x4;

		FretBoardStruct.y1 = -0.5;
		FretBoardStruct.y2 = FretBoardStruct.y1;
		FretBoardStruct.y3 = FretBoardStruct.y1;
		FretBoardStruct.y4 = FretBoardStruct.y1;

		FretBoardStruct.TextureX1 = 0.0;
		FretBoardStruct.TextureX2 = 1.0;

		FretBoardStruct.TextureY1 = 0.0;
		FretBoardStruct.TextureY2 = 1.0;

		FretBoardStruct.Text = Text;

		FretBoardStruct.z1 = (x2 - x1) * -1.5 * i - cCalc;
		FretBoardStruct.z2 = FretBoardStruct.z1;
		FretBoardStruct.z3 = FretBoardStruct.z1 + (x2 - x1) * -1.5;
		FretBoardStruct.z4 = FretBoardStruct.z3;

		FretBoardStruct.alphaBottom = pos2Alpha(-FretBoardStruct.z3 / 5.5);
		FretBoardStruct.alphaTop = pos2Alpha(-FretBoardStruct.z2 / 5.5);

		CEngine::engine().Render3DQuadWithAlpha(FretBoardStruct);
	}
}

void CGamePlay::renderPlayer(CPlayer &player)
{
	auto &lua = CLuaH::Lua();

	CLuaH::multiCallBackParams_t m;

	m.push_back("player 0");

	lua.runEventWithParams("preRenderPlayer", m);

	{
		CEngine::cameraSET usingCamera;
		usingCamera.eyex = 0.0;
		usingCamera.eyey = 0.2;
		usingCamera.eyez = 2.3;
		usingCamera.centerx = 0;
		usingCamera.centery = -0.2;
		usingCamera.centerz = 0;
		usingCamera.upx = 0;
		usingCamera.upy = 1;
		usingCamera.upz = 0;

		CEngine::engine().setCamera(usingCamera);
	}



	double fretboardData[] = { -0.51, 0.51, 0.51, -0.51, -1.0, -1.0, 0.4, 0.4 };


	renderFretBoard(player, fretboardData[0], fretboardData[1], fretboardData[2], fretboardData[3], GPPGame::GuitarPP().loadTexture("data/sprites", "fretboard.tga").getTextId());




	for (int i = 0; i < 5; i++)
	{
		renderIndivdualStrikeButton(i, 0.0, fretsText.strikeLineTexture, 3, player);
	}




	for (auto &n : player.buffer)
	{
		renderNote(n, player);
	}



	CEngine::engine().setColor(1.0, 1.0, 1.0, 1.0);

	{
		CEngine::cameraSET usingCamera;
		usingCamera.eyex = 0.0;
		usingCamera.eyey = 0.0;
		usingCamera.eyez = 2.3;
		usingCamera.centerx = 0;
		usingCamera.centery = 0;
		usingCamera.centerz = 0.0;
		usingCamera.upx = 0;
		usingCamera.upy = 1;
		usingCamera.upz = 0;

		CEngine::engine().setCamera(usingCamera);
	}

	lua.runEventWithParams("posRenderPlayer", m);
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

	//*******************************************************************************************************

	for (auto &p : players)
	{
		renderPlayer(p);
	}

	//*******************************************************************************************************

	CFonts::fonts().drawTextInScreen(std::to_string(CEngine::engine().getFPS()), 0.8, 0.8, 0.1);


}

/*
CGamePlay &CGamePlay::gamePlay()
{
	static CGamePlay game;
	return game;
}*/

CGamePlay::CGamePlay()
{
	speedMp = 2.5;

	fretsTextures = "default";
	fretsText = GPPGame::GuitarPP().frets[fretsTextures];
	fretsText.notesTexture = GPPGame::GuitarPP().loadTexture("data/sprites", "Strums.tga").getTextId();
	fretsText.strikeLineTexture = GPPGame::GuitarPP().loadTexture("data/sprites", "frets.tga").getTextId();
}



