#include "CGamePlay.h"
#include "CEngine.h"
#include "GPPGame.h"
#include "CFonts.h"
#include <common/shader.hpp>

unsigned int programID;
unsigned int MatrixID;
unsigned int TextureID;

void CGamePlay::drawBPMLine(double position, unsigned int Texture, CPlayer &Player)
{
	CEngine::RenderDoubleStruct TempStruct3D;
	double posX1, notaSize, X3, X4, Y3, Y4;
	const double pos = -0.492;
	double rtime = getRunningMusicTime(Player) - position;

	double nCalc = rtime * speedMp;

	nCalc += 0.55;

	const double size = 0.25;

	TempStruct3D.Text = Texture;

	TempStruct3D.x1 = pos;
	TempStruct3D.x2 = pos * -1.0;
	TempStruct3D.x3 = TempStruct3D.x2;
	TempStruct3D.x4 = TempStruct3D.x1;

	TempStruct3D.y1 = -0.4999;
	TempStruct3D.y2 = TempStruct3D.y1;
	TempStruct3D.y3 = -0.4999;
	TempStruct3D.y4 = TempStruct3D.y3;

	TempStruct3D.z1 = nCalc;
	TempStruct3D.z2 = TempStruct3D.z1;
	TempStruct3D.z3 = nCalc + size;
	TempStruct3D.z4 = TempStruct3D.z3;

	TempStruct3D.TextureX1 = 0.0f;
	TempStruct3D.TextureX2 = 1.0f;
	TempStruct3D.TextureY1 = 1.0f;
	TempStruct3D.TextureY2 = 0.0f;

	CEngine::engine().setColor(1.0, 1.0, 1.0, pos2Alpha(-TempStruct3D.z1 / 5.5));
	CEngine::engine().Render3DQuad(TempStruct3D);
}

void CGamePlay::drawBPMLines(CPlayer &Player)
{
	double time = getRunningMusicTime(Player) - 0.5;

	if (Player.Notes.BPM.size() > 0){
		double BPS = 30.0 / Player.Notes.BPM[Player.BPMNowBuffer].lTime;

		int Multi = time / BPS;
		double tCalc = 0.0;

		for (int i = 0; tCalc < 5.0; i++){
			drawBPMLine((Multi * BPS) + tCalc, BPMTextID, Player);

			tCalc = BPS * i;
		}
	}


	CEngine::engine().setColor(1.0, 1.0, 1.0, 1.0);
}

void CGamePlay::renderIndivdualFlame(int id, double pos, unsigned int Texture, int state, double sizeproportion, CPlayer &player)
{
	CEngine::RenderDoubleStruct TempStruct3D;

	double nCalc = pos;

	const double size = 0.25;
	const double position = -0.56;

	TempStruct3D.Text = Texture;

	TempStruct3D.TextureX1 = 1.0 / 4.0 * (double)state;
	TempStruct3D.TextureX2 = 1.0 / 4.0 * (double)state + 1.0 / 4.0;
	TempStruct3D.TextureY1 = 1.0;
	TempStruct3D.TextureY2 = 0.0;

	TempStruct3D.x1 = position + (double(id) * size / 1.15) + (size - size * sizeproportion) / 2.0;
	TempStruct3D.x2 = TempStruct3D.x1 + size * sizeproportion;
	TempStruct3D.x3 = TempStruct3D.x1 + size * sizeproportion;
	TempStruct3D.x4 = TempStruct3D.x1;

	TempStruct3D.y1 = -0.5 + size * sizeproportion;
	TempStruct3D.y2 = TempStruct3D.y1;
	TempStruct3D.y3 = -0.5;
	TempStruct3D.y4 = TempStruct3D.y3;

	TempStruct3D.z1 = nCalc + size * 4.0;
	TempStruct3D.z2 = TempStruct3D.z1;
	TempStruct3D.z3 = TempStruct3D.z1;
	TempStruct3D.z4 = TempStruct3D.z3;

	TempStruct3D.alphaBottom = 1.0;
	TempStruct3D.alphaTop = 0.1;

	CEngine::engine().Render3DQuadWithAlpha(TempStruct3D);
}

void CGamePlay::renderIndivdualStrikeButton3D(int id, double pos, unsigned int Texture, double state, CPlayer &player)
{
	CEngine::RenderDoubleStruct TempStruct3D;
	double rtime = 0.0;

	if (rtime > -5.0)
	{
		double size = 0.2;
		double position = -0.51;

		TempStruct3D.Text = Texture;
		TempStruct3D.TextureX1 = double(id) * 0.2;
		TempStruct3D.TextureX2 = double(id) * 0.2 + 0.2;

		double nCalc = rtime * speedMp;

		nCalc += 0.55;

		if (player.plusEnabled)
		{
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

		CEngine::engine().setColor(1.0, 1.0, 1.0, pos2Alpha(-TempStruct3D.z1 / 5.5));

		//CEngine::engine().Render3DQuad(TempStruct3D);

		CEngine::engine().renderAt(TempStruct3D.x1 + 0.1, -0.52, TempStruct3D.z1 + size);
		//CEngine::engine().setScale(1.2, 1.2, 1.2);
		//CEngine::engine().useShader(programID);

		GPPGame::GuitarPP().triggerBASEOBJ.draw(GPPGame::GuitarPP().strumsTexture3D[id]);
		/*if (state != -10.0)
		{
			CEngine::engine().renderAt(0.0, state, 0.0);
			GPPGame::GuitarPP().triggerOBJ.draw(GPPGame::GuitarPP().loadTexture("data/sprites", "fretboard.tga").getTextId());
		}*/
		CEngine::engine().matrixReset();
		//CEngine::engine().useShader(0);
	}
}


void CGamePlay::renderIndivdualStrikeButton3DStrike(int id, double pos, unsigned int Texture, double state, CPlayer &player)
{
	CEngine::RenderDoubleStruct TempStruct3D;
	double rtime = 0.0;

	if (rtime > -5.0)
	{
		double size = 0.2;
		double position = -0.51;

		TempStruct3D.Text = Texture;
		TempStruct3D.TextureX1 = double(id) * 0.2;
		TempStruct3D.TextureX2 = double(id) * 0.2 + 0.2;

		double nCalc = rtime * speedMp;

		nCalc += 0.55;

		if (player.plusEnabled)
		{
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

		CEngine::engine().setColor(1.0, 1.0, 1.0, pos2Alpha(-TempStruct3D.z1 / 5.5));

		//CEngine::engine().Render3DQuad(TempStruct3D);

		CEngine::engine().renderAt(TempStruct3D.x1 + 0.1, -0.54 + state, TempStruct3D.z1 + size);
		GPPGame::GuitarPP().triggerOBJ.draw(GPPGame::GuitarPP().strumsTexture3D[id]);
		CEngine::engine().matrixReset();
	}
}

void CGamePlay::renderIndivdualStrikeButton(int id, double pos, unsigned int Texture, int state, CPlayer &player)
{
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
	return player.musicRunningTime * gSpeed;
}

double CGamePlay::time2Position(double Time, CPlayer &player)
{
	return ((getRunningMusicTime(player) - Time) * -1.0) / 1.5;
}

double CGamePlay::pos2Alpha(double pos)
{
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

void CGamePlay::renderIndivdualNote(int id, double pos, unsigned int Texture, CPlayer &player)
{
	CEngine::RenderDoubleStruct TempStruct3D;
	double rtime = getRunningMusicTime(player) - pos;

	if (rtime > -5.0)
	{
		double size = 0.2;
		double position = -0.51;

		TempStruct3D.Text = Texture;
		TempStruct3D.TextureX1 = double(id) * 0.2;
		TempStruct3D.TextureX2 = double(id) * 0.2 + 0.2;

		double nCalc = rtime * speedMp;

		nCalc += 0.55;

		if (player.plusEnabled)
		{
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

		//CEngine::engine().Render3DQuad(TempStruct3D);

		CEngine::engine().renderAt(TempStruct3D.x1 + 0.1, -0.5, TempStruct3D.z1 + size);
		//CEngine::engine().setScale(1.2, 1.2, 1.2);
		GPPGame::GuitarPP().noteOBJ.draw(GPPGame::GuitarPP().strumsTexture3D[id]);
		CEngine::engine().matrixReset();
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

	TempStruct3D.y1 = -0.4995;
	TempStruct3D.y2 = TempStruct3D.y1;
	TempStruct3D.y3 = -0.4995;
	TempStruct3D.y4 = TempStruct3D.y3;

	TempStruct3D.z1 = nCalc;
	TempStruct3D.z2 = TempStruct3D.z1;
	TempStruct3D.z3 = nCalc2;
	TempStruct3D.z4 = TempStruct3D.z3;

	TempStruct3D.alphaBottom = pos2Alpha(-TempStruct3D.z3 / 5.5);
	TempStruct3D.alphaTop = pos2Alpha(-TempStruct3D.z2 / 5.5);

	//CEngine::engine().setColor(1.0, 1.0, 1.0, 1.0);

	CEngine::engine().Render3DQuadWithAlpha(TempStruct3D);
}

void CGamePlay::renderNote(CPlayer::NotesData::Note &note, CPlayer &player){
	double time = /*time2Position(*/note.time/*)*/, ltimet = getRunningMusicTime(player);
	for (int i = 0; i < 5; i++){
		if (note.type & (int)pow(2, i)){
			unsigned int texture = fretsText.notesTexture;
			/*if (note.type & notesFlags::nf_doing_slide){
				time = ltimet;
			}*/

			if (note.lTime > 0.0 && note.type & notesFlags::nf_doing_slide)
			{
				renderIndividualLine(i, ltimet, time - ltimet + note.lTime, GPPGame::GuitarPP().loadTexture("data/sprites", "line.tga").getTextId(), player);
			}
			else if (note.type & notesFlags::nf_slide && !(note.type & notesFlags::nf_slide_picked))
			{
				renderIndividualLine(i, time, note.lTime, GPPGame::GuitarPP().loadTexture("data/sprites", "line.tga").getTextId(), player);
			}

			if ((note.type & notesFlags::nf_not_hopo) ^ notesFlags::nf_not_hopo)
			{
				texture = GPPGame::GuitarPP().loadTexture("data/sprites", "HOPOS.tga").getTextId();
			}

			/*if (note.type & notesFlags::plus_mid || note.type & notesFlags::plus_end){
			texture = CTheme::inst().SPR["PlusNote"];
			}*/

			if (!(note.type & notesFlags::nf_picked) && !(note.type & notesFlags::nf_doing_slide)) renderIndivdualNote(i, time, texture, player);
		}
	}
}

void CGamePlay::updatePlayer(CPlayer &player)
{
	auto &notes = player.Notes;
	auto &gNotes = player.Notes.gNotes;
	auto &engine = CEngine::engine();

	player.update();

	player.musicRunningTime += engine.getDeltaTime() * gSpeed;

	player.buffer.clear();

	double musicTime = getRunningMusicTime(player);

	double minendtime = 0.0, minendtimei = notes.notePos;
	//bool minendtimeslide = false;

	bool inslide = false, inslide2 = false;

	for (int ji = 0; ji < 5; ji++)
	{
		if (gNotes[notes.notePos].type & (int)pow(2, ji))
		{
			if (player.notesSlide[ji] != -1)
			{
				inslide = true;
			}
		}
	}

	for (size_t i = notes.notePos, size = gNotes.size(); i < size; i++)
	{
		auto &note = gNotes[i];
		double noteTime = note.time - musicTime;
		double endNoteTime = noteTime + note.lTime;

		double rtime = (musicTime - note.time) * speedMp / gSpeed;

		if (!inslide && !inslide2)
		{
			minendtime = endNoteTime;
			minendtimei = i;

			if (i > 0)
			{
				minendtimei = i - 1;
			}

			if (i > 2)
			{
				minendtimei = i - 2;
			}

			if (i > 5)
			{
				minendtimei = i - 5;
			}
		}

		if (noteTime <= 5.0){
			if ((note.type & notesFlags::nf_picked) == 0)
			{
				player.buffer.push_front(note);
			}
		}

		if (endNoteTime > -1.5 && noteTime < 5.0)
		{
			if (noteTime > -0.1 && noteTime < 0.05 && !(note.type & notesFlags::nf_picked))
			{
				{
					for (int ji = 0; ji < 5; ji++)
					{
						if (note.type & (int)pow(2, ji))
						{
							notes.fretsNotePickedTime[ji] = engine.getTime();
							player.notesSlide[ji] = -1;
						}
					}
				}

				if (note.lTime == 0.0)
				{
					player.doNote(i);
					note.type |= notesFlags::nf_picked;
				}
				else if (!(note.type & notesFlags::nf_doing_slide))
				{
					player.doNote(i);
				}

				if (note.lTime > 0.0)
				{
					note.type |= notesFlags::nf_doing_slide;

					inslide2 = true;

					{
						for (int ji = 0; ji < 5; ji++)
						{
							if (note.type & (int)pow(2, ji))
							{
								player.notesSlide[ji] = i;
							}
						}
					}
				}
			}
		}
		else if (minendtime < -2.0)
		{
			notes.notePos = minendtimei;
		}
		else if (noteTime > 5.0)
		{
			break;
		}
	}

	{
		for (int ji = 0; ji < 5; ji++)
		{
			int64_t id = player.notesSlide[ji];

			if (id != -1)
			{
				auto &note = player.Notes.gNotes[id];
				double noteTime = note.time - musicTime;
				double endNoteTime = noteTime + note.lTime;

				if (endNoteTime < 0.1)
				{
					player.notesSlide[ji] = -1;
					note.type |= notesFlags::nf_picked;
				}
				else
				{
					/*if (id > 0)
					{
						notes.notePos = id - 1;
					}*/

					if (engine.getTime() - player.Notes.fretsNotePickedTime[ji] > 0.05)
					{
						player.Notes.fretsNotePickedTime[ji] = engine.getTime() - 0.05;
					}
				}
			}
		}
	}

}

void CGamePlay::renderFretBoard(CPlayer &player, double x1, double x2, double x3, double x4, unsigned int Text){
	auto positionCalcByT = [this](double p, double prop)
	{
		double cCalc = -p * 5.0;
		double propSpeeed = 5.0 / speedMp;

		cCalc /= propSpeeed;

		cCalc *= 100000.0;
		cCalc = (int)cCalc % (int)(150000 * prop);
		return cCalc / 100000.0;
	};
	
	double musicRunningTime = getRunningMusicTime(player) - 4.0;
	double cCalc = positionCalcByT(musicRunningTime, (x2 - x1));

	for (int i = -2; i < 8; i++)
	{
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

void CGamePlay::renderLyrics()
{
	double time = players[0].musicRunningTime;

	if (songlyrics.size() > songlyricsIndex)
	{
		if (time > songlyrics[songlyricsIndex].tend)
		{
			++songlyricsIndex;
			return;
		}

		if (time > songlyrics[songlyricsIndex].tstart)
		{
			std::string &s = songlyrics[songlyricsIndex].lyric;
			CFonts::fonts().drawTextInScreen(s, CFonts::fonts().getCenterPos(s.size(), 0.1, 0.0), 0.6, 0.1);
		}
	}
}

void CGamePlay::loadSongLyrics(const std::string &song)
{
	songlyrics.clear();
	songlyricsIndex = 0;

	std::string lyricFile = "data/songs/" + song + "/lyrics.srt";
	std::ifstream lyrics(lyricFile);

	auto deduceTime = [](int *a)
	{
		double result = 0;
		result += (double)a[0] * 3600.0;
		result += (double)a[1] * 60.0;
		result += a[2];
		result += (double)a[3] / 1000.0;

		return result;
	};

	std::cout << lyricFile << lyrics.is_open() << std::endl;

	if (lyrics.is_open())
	{
		char temp[1024] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

		int s = 0;
		int id = 0, result = 0;
		int sinc[8] = {0, 0, 0, 0, 0, 0, 0, 0};

		double offset = 0.0;

		lyrics.seekg(0, std::ios::end);
		std::cout << lyrics.tellg() << std::endl;
		lyrics.seekg(0, std::ios::beg);

		lyricLine l;

		while (lyrics.getline(temp, sizeof(temp)))
		{
			if (strlen(temp) == 0)
			{
				continue;
			}

			switch (s++)
			{
			case 0:
				result = sscanf(temp, "%d", &id);

				if (result != 1)
				{
					lyrics.close();
				}
				break;

			case 1:
				result = sscanf(temp, "%d:%d:%d,%d --> %d:%d:%d,%d", &sinc[0], &sinc[1], &sinc[2], &sinc[3], &sinc[4], &sinc[5], &sinc[6], &sinc[7]);

				if (result == 8)
				{
					l.tstart = deduceTime(sinc) + offset;
					l.tend = deduceTime(&sinc[4]) + offset;
				}
				break;

			case 2:
				l.lyric = temp;
				songlyrics.push_back(l);
				break;

			default:
				break;
			}

			if (s > 2)
			{
				s = 0;
			}
		}


	}
}

void CGamePlay::setHyperSpeed(double s)
{
	speedMp = s;
}

void CGamePlay::setMusicSpeed(double s)
{
	gSpeed = s;
}

void CGamePlay::renderPlayer(CPlayer &player)
{
	auto &lua = CLuaH::Lua();

	CLuaH::multiCallBackParams_t m;

	m.push_back("player 0");

	lua.runEventWithParams("preRenderPlayer", m);

	{
		/*CEngine::cameraSET usingCamera;
		usingCamera.eyex = 0.0;
		usingCamera.eyey = 0.2;
		usingCamera.eyez = 2.3;
		usingCamera.centerx = 0;
		usingCamera.centery = -0.2;
		usingCamera.centerz = 0;
		usingCamera.upx = 0;
		usingCamera.upy = 1;
		usingCamera.upz = 0;*/

		CEngine::engine().setCamera(player.playerCamera);
	}


	CEngine::engine().activate3DRender(true);

	double fretboardData[] = { -0.51, 0.51, 0.51, -0.51, -1.0, -1.0, 0.4, 0.4 };


	renderFretBoard(player, fretboardData[0], fretboardData[1], fretboardData[2], fretboardData[3], GPPGame::GuitarPP().loadTexture("data/sprites", "fretboard.tga").getTextId());

	if (showBPMLines) drawBPMLines(player);

	for (int i = 0; i < 5; i++)
	{
		//renderIndivdualStrikeButton(i, 0.0, fretsText.strikeLineTexture, 4, player);
	}

	/*for (int i = 0; i < 5; i++)
	{

		renderIndivdualStrikeButton(i, 0.0, fretsText.strikeLineTexture, 3, player);
		if (player.fretsPressed[i])
			renderIndivdualStrikeButton(i, 0.0, fretsText.strikeLineTexture, 0, player);
		//renderIndivdualNote(i, getRunningMusicTime(player), 0, player);
	}*/


	CEngine::engine().activateNormals(true);

	for (auto &n : player.buffer)
	{
		renderNote(n, player);
	}

	double BPMT = player.Notes.BPM[player.BPMNowBuffer].lTime / 120.0;
	int flamepos = (int)(CEngine::engine().getTime() * 12.0 / BPMT) % 4;


	// ********************************************** STRIKE LINE BTN -
	for (int i = 0; i < 5; i++)
	{
		double pressT = CEngine::engine().getTime() - player.Notes.fretsNotePickedTime[i];

		pressT /= 0.15;

		if (pressT > 1.0 && player.notesSlide[i] == -1)
		{
			continue;
		}

		pressT *= 3;
		int p = (int)floor(pressT + 1) % 3;

		//renderIndivdualStrikeButton(i, 0.0, fretsText.strikeLineTexture, p, player);

		/*if (flamepos < 3) renderIndivdualFlame(i, -0.35, fireText, flamepos + 1, 0.7, player);
		renderIndivdualFlame(i, -0.32, fireText, flamepos, 0.8, player);
		renderIndivdualFlame(i, -0.29, fireText, flamepos, 0.75, player);
		if (flamepos > 0) renderIndivdualFlame(i, -0.25, fireText, flamepos - 1, 0.7, player);*/
	}


	for (int i = 0; i < 5; i++)
	{
		double pressT = CEngine::engine().getTime() - player.Notes.fretsNotePickedTime[i];
		double calcP = (sin(pressT / 0.05)) / 10.0 - 0.05;

		if (pressT > 0.15 && player.notesSlide[i] == -1)
		{
			calcP = -10.0;

			if (player.fretsPressed[i])
			{
				calcP = -0.05;
			}
		}

		renderIndivdualStrikeButton3D(i, 0.0, 0, 0.0, player);
		renderIndivdualStrikeButton3DStrike(i, 0.0, 0, calcP, player);
	}

	CEngine::engine().activateNormals(false);

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

	//CFonts::fonts().drawTextInScreen(std::to_string(player.buffer.size()), 0.0, 0.5, 0.1);

	///////////******************************************

	CEngine::engine().activate3DRender(false);

	auto &engine = CEngine::engine();

	CEngine::RenderDoubleStruct HUDBackground;

	HUDBackground.Text = GPPGame::GuitarPP().loadTexture("data/sprites", "HUD.tga").getTextId();

	double neg = 0.1, negy = 0.05;

	HUDBackground.x1 = -1.0 + neg;
	HUDBackground.x2 = -0.6 + neg;
	HUDBackground.x3 = -0.6 + neg;
	HUDBackground.x4 = -1.0 + neg;

	HUDBackground.y1 = 0.1875 + negy;
	HUDBackground.y2 = 0.1875 + negy;
	HUDBackground.y3 = -0.5 + negy;
	HUDBackground.y4 = -0.5 + negy;

	HUDBackground.TextureX1 = 0.0;
	HUDBackground.TextureX2 = 1.0;
	HUDBackground.TextureY1 = 1.0;
	HUDBackground.TextureY2 = 0.0;

	engine.Render2DQuad(HUDBackground);

	double multi = player.comboToMultiplierWM();
	double circleMultiPercent = multi >= 4.0 ? 100.0 : (multi - floor(multi)) * 100.0;
	double circlePublicAprov = (player.publicAprov / player.maxPublicAprov) * 100.0;
	double circleLoadPercent = (player.plusLoadB / player.maxPlusPower * 5.0) * 100.0;
	double circlePercent = (player.plusPower / player.maxPlusPower) * 100.0;

	engine.setColor(1.0, 1.0, 1.0, 1.0);

	if (circleMultiPercent > 0.0){
		double zeroToOne = circleMultiPercent / 100.0;

		engine.setColor(0.0, 0.4, 1.0, 1.0);
		engine.Render2DCircle(-0.8 + neg, -0.31 + negy, circleMultiPercent, 0.01, 0.041, 200.0 * zeroToOne, 200, player.multiplierBuffer);
	}

	if (player.Notes.gNotes.size() > 0){
		double musicTotalCorrect = (player.correctNotes / player.Notes.gNotes.size()) * 100.0;

		if (musicTotalCorrect > 0.0)
		{
			engine.setColor(0.4, 1.0, 0.4, 1.0);
			engine.Render2DCircle(-0.8 + neg, -0.31 + negy, musicTotalCorrect, 0.05, 0.041, 400.0 * musicTotalCorrect / 100.0, 400, player.correctNotesBuffer);
		}
	}

	if (circlePublicAprov > 0.0){
		double zeroToOne = circlePublicAprov / 100.0;

		engine.setColor(1.0 - 1.0 * zeroToOne, 1.0 * zeroToOne, 0.0, 1.0);
		engine.Render2DCircle(-0.8 + neg, -0.31 + negy, circlePublicAprov, 0.09, 0.041, 600.0 * zeroToOne, 600, player.publicApprovBuffer);
	}

	if (circleLoadPercent > circlePercent){
		if (circleLoadPercent > 0.0){
			double zeroToOne = circleLoadPercent / 100.0;

			engine.setColor(0.4, 1.0, 0.4, 1.0);
			engine.Render2DCircle(-0.8 + neg, -0.31 + negy, circleLoadPercent, 0.13, 0.04, 1000.0 * zeroToOne, 1000, player.plusLoadBuffer);
		}

		if (circlePercent > 0.0){
			double zeroToOne = circlePercent / 100.0;

			engine.setColor(0.0, 1.0, 1.0, 1.0);
			engine.Render2DCircle(-0.8 + neg, -0.31 + negy, circlePercent, 0.13, 0.04, 1000.0 * zeroToOne, 1000, player.plusCircleBuffer);
		}
	}
	else{
		if (circlePercent > 0.0){
			double zeroToOne = circlePercent / 100.0;

			engine.setColor(0.0, 1.0, 1.0, 1.0);
			engine.Render2DCircle(-0.8 + neg, -0.31 + negy, circlePercent, 0.13, 0.04, 1000.0 * zeroToOne, 1000, player.plusCircleBuffer);
		}

		if (circleLoadPercent > 0.0){
			double zeroToOne = circleLoadPercent / 100.0;

			engine.setColor(0.4, 1.0, 0.4, 1.0);
			engine.Render2DCircle(-0.8 + neg, -0.31 + negy, circleLoadPercent, 0.13, 0.04, 1000.0 * zeroToOne, 1000, player.plusLoadBuffer);
		}
	}

	engine.setColor(1.0, 1.0, 1.0, 1.0);

	CFonts::fonts().drawTextInScreen(std::to_string(player.getCombo()), -1.025 + neg, 0.04 + negy, 0.1);
	CFonts::fonts().drawTextInScreen(std::to_string(player.getPoints()), -1.02 + neg, -0.1 + negy, 0.06);
	CFonts::fonts().drawTextInScreen(std::to_string((int)player.comboToMultiplier()), -0.9 + neg, -0.37 + negy, 0.1);

	/////////////***************************************

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
	chartInstruments.clear();

	songlyrics.clear();

	songlyricsIndex = 0;
}

void CGamePlay::render()
{
	CEngine::RenderDoubleStruct RenderData;

	auto &game = GPPGame::GuitarPP();
	/*auto &menu = game.loadTexture("data/sprites", "5168047.tga");

	double prop = (double)menu.getImgWidth() / (double)menu.getImgHeight();

	CEngine::engine().setScale(1.5, 1.5, 1.0);

	double y = 0.3;

	RenderData.x1 = -prop;
	RenderData.x2 = prop;
	RenderData.x3 = prop;
	RenderData.x4 = -prop;

	RenderData.y1 = 1.0 + y;
	RenderData.y2 = 1.0 + y;
	RenderData.y3 = -1.0 + y;
	RenderData.y4 = -1.0 + y;

	RenderData.z1 = 0.0;
	RenderData.z2 = 0.0;
	RenderData.z3 = 0.0;
	RenderData.z4 = 0.0;

	RenderData.TextureX1 = 0.0;
	RenderData.TextureX2 = 1.0;
	RenderData.TextureY1 = 1.0;
	RenderData.TextureY2 = 0.0;

	RenderData.Text = menu.getTextId();*/

	//CEngine::engine().Render2DQuad(RenderData);

	//CEngine::engine().matrixReset();

	//*******************************************************************************************************

	for (auto &p : players)
	{
		renderPlayer(p);
	}

	//*******************************************************************************************************

	CFonts::fonts().drawTextInScreen(std::to_string(CEngine::engine().getFPS()) + " FPS", 0.8, 0.8, 0.1);
}

/*
CGamePlay &CGamePlay::gamePlay()
{
	static CGamePlay game;
	return game;
}*/

CGamePlay::CGamePlay()
{
	speedMp = 2.5; // equivalent to Guitar Hero's hyperspeed

	gSpeed = 1.0; // music speed

	songlyricsIndex = 0;

	programID = LoadShaders("TransformVertexShader.vertexshader", "TextureFragmentShader.fragmentshader");

	// Get a handle for our "MVP" uniform
	MatrixID = CEngine::engine().getUniformLocation(programID, "MVP");
	TextureID = CEngine::engine().getUniformLocation(programID, "myTextureSampler");

	fretsTextures = "default";
	fretsText = GPPGame::GuitarPP().frets[fretsTextures];
	fretsText.notesTexture = GPPGame::GuitarPP().loadTexture("data/sprites", "Strums.tga").getTextId();
	fretsText.strikeLineTexture = GPPGame::GuitarPP().loadTexture("data/sprites", "frets.tga").getTextId();
	fireText = GPPGame::GuitarPP().loadTexture("data/sprites", "flamea.tga").getTextId();

	BPMLineText = "v.tga";

	BPMTextID = GPPGame::GuitarPP().loadTexture("data/sprites", BPMLineText).getTextId();

	showBPMLines = true;


	{
		CMenu::menuOpt opt;

		opt.text = "Voltar";
		opt.y = 0.4;
		opt.x = -0.5;
		opt.size = 0.075;
		opt.group = 1;
		opt.status = 0;
		opt.type = CMenu::menusOPT::textbtn;
		opt.goback = true;

		/*exitModuleOpt = */moduleMenu.addOpt(opt);
	}

	{
		CMenu::menuOpt opt;

		opt.text = "Sair";
		opt.y = -0.4;
		opt.x = -0.5;
		opt.size = 0.075;
		opt.group = 1;
		opt.status = 0;
		opt.type = CMenu::menusOPT::textbtn;
		opt.goback = true;

		exitModuleOpt = moduleMenu.addOpt(opt);
	}
}



