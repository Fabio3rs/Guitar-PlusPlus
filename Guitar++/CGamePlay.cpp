#include "CGamePlay.h"
#include "CEngine.h"
#include "GPPGame.h"
#include "CFonts.h"
#include <array>
#include <GLFW/glfw3.h>

int gpppowi(int b, int e)
{
	return static_cast<int>(pow(b, e));
}

const int CGamePlay::notesFlagsConst[5] = { gpppowi(2, 0), gpppowi(2, 1), gpppowi(2, 2), gpppowi(2, 3), gpppowi(2, 4) };

void CGamePlay::drawBPMLine(double position, unsigned int Texture, CPlayer &Player)
{
	CEngine::RenderDoubleStruct TempStruct3D;
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

	double alpha = pos2Alpha(-TempStruct3D.z1 / 6.5);

	if (alpha <= 0.0)
	{
		return;
	}

	CEngine::engine().setColor(1.0, 1.0, 1.0, alpha);
	CEngine::engine().Render3DQuad(TempStruct3D);
}

double CGamePlay::fretboardPositionCalcByT(double time, double prop, double *max)
{
	const double size = 2.1 * 1.33333333;

	if (max)
		*max = size * prop;

	double cCalc = -time * 5.0;
	double propSpeeed = 5.0 / speedMp;

	cCalc /= propSpeeed;

	cCalc *= 1000000.0;
	cCalc = static_cast<double>((int64_t)cCalc % (int64_t)((size * 1000000) * prop));
	return cCalc / 1000000.0;
}

double CGamePlay::getBPMAt(CPlayer &player, double time)
{
	double result = 120.0, resultI = 0;

	for (int i = 0, size = player.Notes.BPM.size(); i < size; i++)
	{
		auto &BPMn = player.Notes.BPM[i];

		if (BPMn.time <= time)
		{
			result = BPMn.lTime;
		}
		else
		{
			break;
		}

		resultI = i;
	}

	return result;
}

CPlayer::NotesData::Note CGamePlay::getBPMAtStruct(CPlayer &player, double time)
{
	CPlayer::NotesData::Note result;

	for (auto &BPMn : player.Notes.BPM)
	{
		if (BPMn.time <= time)
		{
			result = BPMn;
		}
		else
		{
			break;
		}
	}

	return result;
}

std::vector<CPlayer::NotesData::Note>::iterator CGamePlay::getBPMAtIt(CPlayer &player, double time)
{
	std::vector<CPlayer::NotesData::Note>::iterator result;

	for (auto it = player.Notes.BPM.begin(); it != player.Notes.BPM.end(); it++)
	{
		if ((*it).time < time)
		{
			result = it;
		}
		else
		{
			break;
		}
	}

	return result;
}

void CGamePlay::alertLVLUP(CPlayer &player)
{
	CEngine &engine = CEngine::engine();

	double delta = engine.getTime() - player.lvlUpAlert.startTime;

	double y = 0.6;

	if (delta <= 0.2) {
		double scale = delta / 0.2;

		engine.setScale(scale, scale, 1.0);
	}
	else if (delta >= 0.2 && delta <= 1.5) {


	}
	else if (delta >= 1.5 && delta <= 2.5) {
		y += (delta - 1.5) / 1.0;
	}

	std::string lvlUPMsg = "Level UP! ";
	lvlUPMsg += std::to_string(player.getLevel());

	CFonts::fonts().drawTextInScreen(lvlUPMsg,
		CFonts::fonts().getCenterPos(lvlUPMsg, 0.1, 0.0), y, 0.1);
	engine.matrixReset();
}

void CGamePlay::alertNotesStreak(CPlayer &player)
{
	CEngine &engine = CEngine::engine();

	double delta = engine.getTime() - player.notesStreakAlert.startTime;

	double y = 0.6;

	if (delta <= 0.2) {
		double scale = delta / 0.2;

		engine.setScale(scale, scale, 1.0);
	}
	else if (delta >= 0.2 && delta <= 1.5) {


	}
	else if (delta >= 1.5 && delta <= 2.5) {
		y += (delta - 1.5) / 1.0;
	}

	std::string notesStreakMsg = std::to_string(player.notesStreakAlert.status);
	notesStreakMsg += " Notes Streak";

	CFonts::fonts().drawTextInScreen(notesStreakMsg,
		CFonts::fonts().getCenterPos(notesStreakMsg, 0.1, 0.0), y, 0.1);
	engine.matrixReset();
}

bool CGamePlay::alertTest(CFonts::textAlert &t)
{
	CEngine &engine = CEngine::engine();

	double delta = engine.getTime() - t.startTime;

	double y = 0.7;

	if (delta <= 0.2) {
		double scale = delta / 0.2;

		engine.setScale(scale, scale, 1.0);
	}
	else if (delta >= 0.2 && delta <= 1.5) {


	}
	else if (delta >= 1.5 && delta <= 2.5) {
		y += (delta - 1.5) / 1.0;
	}

	CFonts::fonts().drawTextInScreen(t.msg,
		CFonts::fonts().getCenterPos(t.msg, 0.1, 0.0), y, 0.1);
	engine.matrixReset();

	return delta <= 2.3;
}

size_t CGamePlay::getBPMAtI(CPlayer &player, double time)
{
	double result = 120.0;
	size_t resultI = 0;

	for (size_t i = 0, size = player.Notes.BPM.size(); i < size; i++)
	{
		auto &BPMn = player.Notes.BPM[i];

		if (BPMn.time <= time)
		{
			result = BPMn.lTime;
		}
		else
		{
			break;
		}

		resultI = i;
	}

	return resultI;
}

void CGamePlay::drawBPMLines(CPlayer &Player)
{
	static std::vector <CPlayer::NotesData::Note> BPMValuesdata;
	BPMl.clear();
	BPMValuesdata.clear();
	BPMl.useColors = true;
	BPMl.autoEnDisaColors = false;
	BPMl.texture = BPMTextID;
	double mscRunnTime = getRunningMusicTime(Player);
	const double time = mscRunnTime - 0.5;

	auto calcQuad = [&](double position, double runTime, CPlayer &Player)
	{
		static CEngine::RenderDoubleStruct TempStruct3D;
		double rtime = runTime - position;

		double nCalc = rtime * speedMp;

		nCalc += 0.75;

		double alpha = pos2Alpha(-nCalc / 5.8);

		if (alpha <= 0.0)
		{
			return false;
		}

		const double pos = -0.492;

		const double size = 0.05;

		static staticCallFunc rendr([&]()
		{
			TempStruct3D.x1 = pos;
			TempStruct3D.x2 = pos * -1.0;
			TempStruct3D.x3 = TempStruct3D.x2;
			TempStruct3D.x4 = TempStruct3D.x1;

			TempStruct3D.y1 = -0.4995;
			TempStruct3D.y2 = TempStruct3D.y1;
			TempStruct3D.y3 = -0.4995;
			TempStruct3D.y4 = TempStruct3D.y3;

			TempStruct3D.TextureX1 = 0.0f;
			TempStruct3D.TextureX2 = 1.0f;
			TempStruct3D.TextureY1 = 1.0f;
			TempStruct3D.TextureY2 = 0.0f;
		});

		TempStruct3D.z1 = nCalc;
		TempStruct3D.z2 = TempStruct3D.z1;
		TempStruct3D.z3 = nCalc + size;
		TempStruct3D.z4 = TempStruct3D.z3;

		TempStruct3D.alphaBottom = alpha;
		TempStruct3D.alphaTop = alpha;

		CEngine::pushQuad(BPMl, TempStruct3D);

		return true;
	};

	double BBPM = 0.0;
	double minTime = 0;

	const auto bpmBuffSize = Player.Notes.BPM.size();

	if (bpmBuffSize > 0)
	{
		const double mtime = (time - 1.0);
		int BPMnowbuff = Player.BPMNowBuffer/*getBPMAtI(Player, (mtime > 0.0) ? mtime : 0.0)*/;

		if (static_cast<size_t>(BPMnowbuff) >= bpmBuffSize)
		{
			BPMnowbuff = bpmBuffSize - 1;
		}

		if (BPMnowbuff < 0)
		{
			BPMnowbuff = 0;
		}

		if (mtime >= 0.0)
		{
			int nbuff = BPMnowbuff + 1;
			if (bpmBuffSize > static_cast<size_t>(nbuff))
			{
				if (((mscRunnTime - 1.5) > (Player.Notes.BPM[nbuff].time)))
				{
					BPMnowbuff = nbuff;
					Player.BPMNowBuffer = BPMnowbuff;
				}
			}
		}
		else
		{
			BPMnowbuff = 0;
		}

		double BPS = 60.0 / Player.Notes.BPM[BPMnowbuff].lTime;

		double blinetime = mtime - Player.Notes.BPM[BPMnowbuff].time;

		if (blinetime >= 0.0)
		{
			blinetime /= BPS;
		}
		else
		{
			blinetime = 0.0;
		}

		double tCalc = 0.0;
		double timeToSum = Player.Notes.BPM[BPMnowbuff].time;

		int64_t bpmMultiplier = static_cast<int64_t>(floor(blinetime));
		//double bpmMultiplierd = static_cast<double>(bpmMultiplier);

		double chartEnd = Player.Notes.getChartEnd(1.0);

		for (int i = 0; tCalc < 9.0; i++, bpmMultiplier++)
		{
			blinetime = timeToSum + BPS * bpmMultiplier;

			if (blinetime > chartEnd)
			{
				break;
			}

			{
				int nbuff = BPMnowbuff + 1;
				if (bpmBuffSize > static_cast<size_t>(nbuff))
				{
					if (blinetime >= Player.Notes.BPM[nbuff].time)
					{
						BPMnowbuff = nbuff;

						bpmMultiplier = 0;
						auto &BPMNow = Player.Notes.BPM[BPMnowbuff];

						blinetime = timeToSum = BPMNow.time;
						BPS = 60.0 / BPMNow.lTime;
					}
				}
			}

			if (!calcQuad(blinetime, mscRunnTime, Player))
			{
				break;
			}

			tCalc += BPS;

			if ((mscRunnTime - blinetime) >= 2.0)
			{
				Player.Notes.BPMMinPosition = blinetime;
			}
		}

		if (BPMl.vArray.size() > 0)
			CEngine::engine().drawTrianglesWithAlpha(BPMl);

		if (showBPMVlaues)
		{
			std::vector<CPlayer::NotesData::Note>::iterator nullit;

			if (minTime < 0.0)
				minTime = 0.0;

			auto it = getBPMAtIt(Player, minTime);

			if (it == nullit)
			{
				it = Player.Notes.BPM.begin();
			}

			if (it != Player.Notes.BPM.end())
			{
				for (; it != Player.Notes.BPM.end(); it++)
				{
					if ((*it).time < (mscRunnTime + 5.0))
					{
						BPMValuesdata.push_back(*it);
					}
					else
					{
						break;
					}
				}
			}

			if (BPMValuesdata.size() > 0)
			{
				double runt = getRunningMusicTime(Player);

				CEngine::engine().activate3DRender(false);

				for (auto &BPMv : BPMValuesdata)
				{
					double nCalc = (runt - BPMv.time) * speedMp;

					nCalc += 0.55;

					CFonts::fonts().draw3DTextInScreen(std::to_string(BPMv.lTime), 0.492, -0.5, nCalc, 0.1, 0.1, 0.0);
				}

				CEngine::engine().activate3DRender(true);
			}
		}
	}
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
	static double lp[3] = {0.0, 0.0, 0.0};

	if (rtime > -5.0)
	{
		const double idd = id;

		const double size = 0.2;
		const double position = -0.51;

		auto form = [size, position](double idd)
		{
			return position + (idd * size / 48.0) + (idd * size);
		};

		const static double xdata[] = { form(0), form(1), form(2), form(3), form(4) };

		TempStruct3D.Text = Texture;
		TempStruct3D.TextureX1 = idd * 0.2;
		TempStruct3D.TextureX2 = idd * 0.2 + 0.2;

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

		TempStruct3D.x1 = xdata[id]/*position + (double(id) * size / 48.0) + (double(id) * size)*/;
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
		/*
		double alpha = pos2Alpha(-TempStruct3D.z1 / 5.8);

		if (alpha <= 0.0)
		{
			return;
		}
*/
		//CEngine::engine().setColor(1.0, 1.0, 1.0, alpha);

		//CEngine::engine().Render3DQuad(TempStruct3D);
		//if (id == 0)
		{
			lp[0] = TempStruct3D.x1 + 0.1;
			lp[1] = -0.4992;
			lp[2] = TempStruct3D.z1 + size;
			CEngine::engine().renderAt(TempStruct3D.x1 + 0.1, -0.4992, TempStruct3D.z1 + size);
		}
		/*else
		{
			CEngine::engine().renderAt(((TempStruct3D.x1 + 0.1) - lp[0]), (-0.4992) - lp[1], (TempStruct3D.z1 + size) - lp[2]);
			lp[0] = TempStruct3D.x1 + 0.1;
			lp[1] = -0.4992;
			lp[2] = TempStruct3D.z1 + size;
		}*/
		//CEngine::engine().setScale(1.2, 1.2, 1.2);
		//CEngine::engine().useShader(programID);

		auto texts = GPPGame::GuitarPP().sbaseTexture3D;

		GPPGame::GuitarPP().triggerBASEOBJ.draw(player.plusEnabled ? texts[5] : texts[id], false);
		/*if (state != -10.0)
		{
			CEngine::engine().renderAt(0.0, state, 0.0);
			GPPGame::GuitarPP().triggerOBJ.draw(GPPGame::GuitarPP().loadTexture("data/sprites", "fretboard.tga").getTextId());
		}*/

		//if (id == 4)
			//CEngine::engine().matrixReset();
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

		auto form = [size, position](double idd)
		{
			return position + (idd * size / 48.0) + (idd * size);
		};

		const static double xdata[] = { form(0), form(1), form(2), form(3), form(4) };

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

		TempStruct3D.x1 = xdata[id]/*position + (double(id) * size / 48.0) + (double(id) * size)*/;
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

		/*
		double alpha = pos2Alpha(-TempStruct3D.z1 / 5.8);

		if (alpha <= 0.0)
		{
			return;
		}
		*/
		//CEngine::engine().setColor(1.0, 1.0, 1.0, alpha);

		//CEngine::engine().Render3DQuad(TempStruct3D);
		auto texts = GPPGame::GuitarPP().striggerTexture3D;

		CEngine::engine().renderAt(TempStruct3D.x1 + 0.1, -0.5 + state, TempStruct3D.z1 + size);
		GPPGame::GuitarPP().triggerOBJ.draw(player.plusEnabled ? texts[5] : texts[id], false);
		//CEngine::engine().matrixReset();
	}
}

/*void CGamePlay::renderIndivdualStrikeButton(int id, double pos, unsigned int Texture, int state, CPlayer &player)
{
	CEngine::RenderDoubleStruct TempStruct3D;

	//auto &frets = GPPGame::GuitarPP().frets[fretsTextures];

	double nCalc = pos;

	const double size = 0.25;
	const double position = -0.56;

	auto form = [size, position](double idd)
	{
		return position + (idd * size / 1.15);
	};

	const static double xdata[] = { form(0), form(1), form(2), form(3), form(4) };

	TempStruct3D.Text = Texture;

	TempStruct3D.TextureX1 = fretsText.FretIMGPos[fretsText.sAIDTI(state, id)].x * fretsText.columnSize;
	TempStruct3D.TextureX2 = TempStruct3D.TextureX1 + fretsText.columnSize;
	TempStruct3D.TextureY1 = 1.0 - fretsText.FretIMGPos[fretsText.sAIDTI(state, id)].y * fretsText.lineFretSize;
	TempStruct3D.TextureY2 = TempStruct3D.TextureY1 - fretsText.lineFretSize;

	TempStruct3D.x1 = xdata[id]/*position + (double(id) * size / 1.15)*-/;
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
}*/


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

	const double minPosMinAlpha = 1.6/*1.5*/, maxPosMaxAlpha = 0.6, dif = minPosMinAlpha - maxPosMaxAlpha;

	if (pos < maxPosMaxAlpha)
	{
		return 1.0;
	}

	if (pos > minPosMinAlpha)
	{
		return 0.0;
	}

	result = (1.0 / dif) * (minPosMinAlpha - pos);

	if (result > 1.0)
		return 1.0;

	return result;
}

void CGamePlay::renderHoposLight()
{
	double size = 0.2;
	double position = -0.51;

	CEngine::RenderDoubleStruct TempStruct3D;

	TempStruct3D.Text = hopoLightText;
	TempStruct3D.TextureX1 = 0.0;
	TempStruct3D.TextureX2 = 1.0;
	TempStruct3D.TextureY1 = 1.0;
	TempStruct3D.TextureY2 = 0.0;

	//CFonts::fonts().drawTextInScreen(std::to_string(hopostp.size()), 0.0, 0.5, 0.1);

	for (auto &hp : hopostp)
	{
		TempStruct3D.x1 = hp.x;
		TempStruct3D.x2 = TempStruct3D.x1 + size;
		TempStruct3D.x3 = TempStruct3D.x1 + size;
		TempStruct3D.x4 = TempStruct3D.x1;

		TempStruct3D.y1 = hp.y;
		TempStruct3D.y2 = TempStruct3D.y1;
		TempStruct3D.y3 = hp.y;
		TempStruct3D.y4 = TempStruct3D.y3;

		TempStruct3D.z1 = hp.z;
		TempStruct3D.z2 = TempStruct3D.z1;
		TempStruct3D.z3 = hp.z + size * 2.0;
		TempStruct3D.z4 = TempStruct3D.z3;


		double alpha = pos2Alpha(-TempStruct3D.z1 / 5.8);


		if (alpha <= 0.0)
		{
			continue;
		}

		CEngine::engine().setColor(1.0, 1.0, 1.0, alpha);

		CEngine::engine().Render3DQuad(TempStruct3D);
	}


	CEngine::engine().setColor(1.0, 1.0, 1.0, 1.0);
	hopostp.clear();
}

void CGamePlay::renderIndivdualNoteShadow(int id, double pos, unsigned int Texture, bool tail, CPlayer &player)
{
	CEngine::RenderDoubleStruct TempStruct3D;
	double rtime = getRunningMusicTime(player) - pos;

	bool rotated = false;

	if (rtime > -5.0)
	{
		double size = 0.2;
		double position = -0.51;

		auto form = [size, position](double idd)
		{
			return position + (double(idd) * size / 48.0) + (double(idd) * size);
		};

		const static double xdata[] = { form(0), form(1), form(2), form(3), form(4) };

		//TempStruct3D.Text = GPPGame::GuitarPP().loadTexture("data/sprites", "hopolght.tga").getTextId();
		/*TempStruct3D.TextureX1 = double(id) * 0.2;
		TempStruct3D.TextureX2 = double(id) * 0.2 + 0.2;*/
		TempStruct3D.TextureX1 = 0.0;
		TempStruct3D.TextureX2 = 1.0;

		double nCalc = rtime * speedMp;

		nCalc += 0.55;

		TempStruct3D.TextureY1 = 1.0;
		TempStruct3D.TextureY2 = 0.0;

		TempStruct3D.x1 = xdata[id]/*position + (double(id) * size / 48.0) + (double(id) * size)*/;
		TempStruct3D.x2 = TempStruct3D.x1 + size;
		TempStruct3D.x3 = TempStruct3D.x1 + size;
		TempStruct3D.x4 = TempStruct3D.x1;

		TempStruct3D.y1 = -0.4;
		TempStruct3D.y2 = TempStruct3D.y1;
		TempStruct3D.y3 = -0.4;
		TempStruct3D.y4 = TempStruct3D.y3;

		TempStruct3D.z1 = nCalc;
		TempStruct3D.z2 = TempStruct3D.z1;
		TempStruct3D.z3 = nCalc + size * 2.0;
		TempStruct3D.z4 = TempStruct3D.z3;

		CEngine::engine().renderAt(TempStruct3D.x1 + 0.1, -0.5, TempStruct3D.z1 + size);

		size_t plusPos = player.Notes.plusPos;

		if (plusPos < player.Notes.gPlus.size())
		{
			auto &p = player.Notes.gPlus[plusPos];

			if (p.time <= pos && pos < (p.time + p.lTime))
			{
				CEngine::engine().Rotate(player.rangle, 0.0, 1.0, 0.0);
				rotated = true;
			}
		}

		//auto texts = (Texture == GPPGame::GuitarPP().HOPOSText) ? GPPGame::GuitarPP().hopoTexture3D : GPPGame::GuitarPP().strumsTexture3D;

		GPPGame::GuitarPP().noteOBJ.draw(GPPGame::GuitarPP().strumsTexture3D[id], false);
	}

	if (rotated)
		CEngine::engine().matrixReset();
}

void CGamePlay::renderIndivdualNote(int id, double pos, unsigned int Texture, int type, CPlayer &player)
{
	CEngine::RenderDoubleStruct TempStruct3D;
	double rtime = getRunningMusicTime(player) - pos;

	bool rotated = false;

	if (rtime > -5.0)
	{
		double size = 0.2;
		double position = -0.51;

		auto form = [size, position](double idd)
		{
			return position + (double(idd) * size / 48.0) + (double(idd) * size);
		};

		const static double xdata[] = { form(0), form(1), form(2), form(3), form(4) };
		
		//TempStruct3D.Text = GPPGame::GuitarPP().loadTexture("data/sprites", "hopolght.tga").getTextId();
		/*TempStruct3D.TextureX1 = double(id) * 0.2;
		TempStruct3D.TextureX2 = double(id) * 0.2 + 0.2;*/
		TempStruct3D.TextureX1 = 0.0;
		TempStruct3D.TextureX2 = 1.0;

		double nCalc = rtime * speedMp;

		nCalc += 0.55;

		TempStruct3D.TextureY1 = 1.0;
		TempStruct3D.TextureY2 = 0.0;

		TempStruct3D.x1 = xdata[id]/*position + (double(id) * size / 48.0) + (double(id) * size)*/;
		TempStruct3D.x2 = TempStruct3D.x1 + size;
		TempStruct3D.x3 = TempStruct3D.x1 + size;
		TempStruct3D.x4 = TempStruct3D.x1;

		TempStruct3D.y1 = -0.49;
		TempStruct3D.y2 = TempStruct3D.y1;
		TempStruct3D.y3 = -0.49;
		TempStruct3D.y4 = TempStruct3D.y3;

		TempStruct3D.z1 = nCalc;
		TempStruct3D.z2 = TempStruct3D.z1;
		TempStruct3D.z3 = nCalc + size * 2.0;
		TempStruct3D.z4 = TempStruct3D.z3;


		double alpha = pos2Alpha(-TempStruct3D.z1 / 5.8);

		if (alpha <= 0.0)
		{
			return;
		}

		CEngine::engine().setColor(1.0, 1.0, 1.0, alpha);

		//CEngine::engine().Render3DQuad(TempStruct3D);

		if (Texture == GPPGame::GuitarPP().HOPOSText)
		{
			gppVec3f vec3data;
			vec3data.x = static_cast<float>(TempStruct3D.x1);
			vec3data.y = -0.462f;
			vec3data.z = static_cast<float>(TempStruct3D.z1);


			hopostp.push_front(vec3data);
		}

		CEngine::engine().renderAt(TempStruct3D.x1 + 0.1, -0.5, TempStruct3D.z1 + size);

		size_t plusPos = player.Notes.plusPos;

		if (plusPos < player.Notes.gPlus.size())
		{
			auto &p = player.Notes.gPlus[plusPos];
			
			if (p.time <= pos && pos < (p.time + p.lTime))
			{
				CEngine::engine().Rotate(player.rangle, 0.0, 1.0, 0.0);
				rotated = true;
				/*

				plusNoteLight.position[0] = TempStruct3D.x1;
				plusNoteLight.position[1] = -0.2;
				plusNoteLight.position[2] = TempStruct3D.z1;
				plusNoteLight.position[3] = TempStruct3D.z1 - size * 5.0;

				plusNoteLight.direction[0] = TempStruct3D.x1;
				plusNoteLight.direction[1] = -0.48;
				plusNoteLight.direction[2] = TempStruct3D.z1 + size;
				plusNoteLight.direction[3] = TempStruct3D.z1 + size * 4.0;
				plusNoteLight.angle = 120.0;

				CEngine::engine().setLight(plusNoteLight, 3);
				CEngine::engine().activateLight(3, true);*/
			}
		}

		auto texts = (Texture == GPPGame::GuitarPP().HOPOSText) ? GPPGame::GuitarPP().hopoTexture3D : GPPGame::GuitarPP().strumsTexture3D;

		if (type & noteTap)
		{
			texts = GPPGame::GuitarPP().tapTexture3D;
		}

		/*if (Texture == GPPGame::GuitarPP().HOPOSText)
		{
			hoposLight.position[0] = TempStruct3D.x1;
			hoposLight.position[1] = -0.2;
			hoposLight.position[2] = TempStruct3D.z1;
			hoposLight.position[3] = TempStruct3D.z1 - size * 5.0;

			hoposLight.direction[0] = TempStruct3D.x1;
			hoposLight.direction[1] = -0.48;
			hoposLight.direction[2] = TempStruct3D.z1 + size;
			hoposLight.direction[3] = TempStruct3D.z1 + size * 4.0;
			hoposLight.angle = 120.0;

			CEngine::engine().setLight(hoposLight, 2);
			CEngine::engine().activateLight(2, true);
		}*/


		//CEngine::engine().setScale(1.2, 1.2, 1.2);
		GPPGame::GuitarPP().noteOBJ.draw(player.plusEnabled ? texts[5] : texts[id], false);
		//CEngine::engine().matrixReset();

		/*if (Texture == GPPGame::GuitarPP().HOPOSText)
		{
			CEngine::engine().activateLight(2, false);
		}*/
	}

	if (rotated)
		CEngine::engine().matrixReset();
}

void CGamePlay::renderOpenNote(double pos, unsigned int Texture, int type, CPlayer &player)
{
	CEngine::RenderDoubleStruct TempStruct3D;
	double rtime = getRunningMusicTime(player) - pos;

	bool rotated = false;

	if (rtime > -5.0)
	{
		double size = 0.2;
		double position = -0.51;

		auto form = [size, position](double idd)
		{
			return position + (double(idd) * size / 48.0) + (double(idd) * size);
		};

		const static double xdata[] = { form(0), form(1), form(2), form(3), form(4) };

		//TempStruct3D.Text = GPPGame::GuitarPP().loadTexture("data/sprites", "hopolght.tga").getTextId();
		/*TempStruct3D.TextureX1 = double(id) * 0.2;
		TempStruct3D.TextureX2 = double(id) * 0.2 + 0.2;*/
		TempStruct3D.TextureX1 = 0.0;
		TempStruct3D.TextureX2 = 1.0;

		double nCalc = rtime * speedMp;

		nCalc += 0.55;

		TempStruct3D.TextureY1 = 1.0;
		TempStruct3D.TextureY2 = 0.0;

		TempStruct3D.x1 = xdata[2]/*position + (double(id) * size / 48.0) + (double(id) * size)*/;
		TempStruct3D.x2 = TempStruct3D.x1 + size;
		TempStruct3D.x3 = TempStruct3D.x1 + size;
		TempStruct3D.x4 = TempStruct3D.x1;

		TempStruct3D.y1 = -0.46;
		TempStruct3D.y2 = TempStruct3D.y1;
		TempStruct3D.y3 = -0.46;
		TempStruct3D.y4 = TempStruct3D.y3;

		TempStruct3D.z1 = nCalc;
		TempStruct3D.z2 = TempStruct3D.z1;
		TempStruct3D.z3 = nCalc + size * 2.0;
		TempStruct3D.z4 = TempStruct3D.z3;

		bool notHOPO = (type & notesFlags::nf_not_hopo);

		if (!notHOPO)
		{
			for (int i = 0; i < 5; ++i)
			{
				gppVec3f vec3data;
				vec3data.x = static_cast<float>(xdata[i]);
				vec3data.y = -0.47f;
				vec3data.z = static_cast<float>(TempStruct3D.z1);

				hopostp.push_front(vec3data);
			}
		}

		double alpha = pos2Alpha(-TempStruct3D.z1 / 5.8);

		if (alpha <= 0.0)
		{
			return;
		}

		CEngine::engine().setColor(1.0, 1.0, 1.0, alpha);

		CEngine::engine().renderAt(0.0, -0.5, TempStruct3D.z1 + size);

		unsigned int text = 0;

		if (player.plusEnabled)
		{
			text = notHOPO ? GPPGame::GuitarPP().openNotePTexture3D : GPPGame::GuitarPP().openNoteHOPOPTexture3D;
		}
		else
		{
			text = notHOPO ? GPPGame::GuitarPP().openNoteTexture3D : GPPGame::GuitarPP().openNoteHOPOTexture3D;
		}

		GPPGame::GuitarPP().openNoteOBJ.draw(text, false);
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

	auto form = [size, position](double idd)
	{
		return position + (idd * size / 48.0) + (idd * size);
	};

	const static double xdata[] = { form(0), form(1), form(2), form(3), form(4) };

	double nCalc = rtime * speedMp;
	double nCalc2 = rtime2 * speedMp;

	nCalc += 0.55 + size;
	nCalc2 += 0.55 + size * 1.5;

	TempStruct3D.Text = Texture;
	TempStruct3D.TextureX1 = 0.0;
	TempStruct3D.TextureX2 = 1.0;
	TempStruct3D.TextureY1 = 1.0;
	TempStruct3D.TextureY2 = 0.0;

	TempStruct3D.x1 = xdata[id]/*position + (double(id) * size / 48.0) + (double(id) * size)*/;
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

void CGamePlay::renderTimeOnNote(double pos, double time, CPlayer &player)
{
	CEngine::RenderDoubleStruct TempStruct3D;
	double rtime = getRunningMusicTime(player) - pos;

	if (rtime > -5.0)
	{
		double size = 0.2;
		double position = -0.51;

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


		TempStruct3D.y1 = -0.5;
		TempStruct3D.y2 = TempStruct3D.y1;
		TempStruct3D.y3 = -0.5;
		TempStruct3D.y4 = TempStruct3D.y3;

		TempStruct3D.z1 = nCalc;
		TempStruct3D.z2 = TempStruct3D.z1;
		TempStruct3D.z3 = nCalc + size * 2.0;
		TempStruct3D.z4 = TempStruct3D.z3;


		double alpha = pos2Alpha(-TempStruct3D.z1 / 5.8);

		if (alpha <= 0.0)
		{
			return;
		}

		CEngine::engine().setColor(1.0, 1.0, 1.0, alpha);

		//CEngine::engine().Render3DQuad(TempStruct3D);
		std::string timestr = std::to_string(time);

		CFonts::fonts().draw3DTextInScreen(timestr, CFonts::fonts().getCenterPos(timestr, 0.2, 0.0), -0.4992, TempStruct3D.z1, 0.2, 0.0, -0.2);
	}
}

void CGamePlay::renderTailsBuffer(CPlayer &player)
{
	CEngine::RenderDoubleStruct TempStruct3D;

	unsigned int text = GPPGame::GuitarPP().lineText;

	const double size = 0.2;
	const double position = -0.51;

	auto form = [&](int i)
	{
		return position + (double(i) * size / 48.0) + (double(i) * size);
	};

	static const double posx[] = { form(0), form(1), form(2), form(3), form(4) };
	static const double posxpsize[] = { posx[0] + size, posx[1] + size, posx[2] + size, posx[3] + size, posx[4] + size };

	TempStruct3D.Text = text;
	TempStruct3D.TextureX1 = 0.0;
	TempStruct3D.TextureX2 = 1.0;
	TempStruct3D.TextureY1 = 1.0;
	TempStruct3D.TextureY2 = 0.0;

	TempStruct3D.y1 = -0.49;
	TempStruct3D.y2 = TempStruct3D.y1;
	TempStruct3D.y3 = -0.49;
	TempStruct3D.y4 = TempStruct3D.y3;

	for (auto &t : player.tailsData)
	{
		for (int i = 0; i < 5; i++)
		{
			if (t.type & notesFlagsConst[i])
			{
				TempStruct3D.x1 = posx[i];
				TempStruct3D.x2 = posxpsize[i];
				TempStruct3D.x3 = TempStruct3D.x2;
				TempStruct3D.x4 = TempStruct3D.x1;

				TempStruct3D.z1 = t.top;
				TempStruct3D.z2 = TempStruct3D.z1;
				TempStruct3D.z3 = t.bottom;
				TempStruct3D.z4 = TempStruct3D.z3;

				TempStruct3D.alphaBottom = pos2Alpha(-TempStruct3D.z3 / 5.5);
				TempStruct3D.alphaTop = pos2Alpha(-TempStruct3D.z2 / 5.5);

				//CEngine::engine().setColor(1.0, 1.0, 1.0, 1.0);

				CEngine::engine().Render3DQuadWithAlpha(TempStruct3D);
			}
		}
	}
}

void CGamePlay::addTailToBuffer(const CPlayer::NotesData::Note &note, double pos1, double pos2, double runningTime, CPlayer &player)
{
	//double runningTime = getRunningMusicTime(player);
	double rtime = runningTime - pos1;
	double rtime2 = runningTime - (pos1 + pos2);

	const double size = 0.2;
	const double position = -0.51;

	double nCalc = rtime * speedMp;
	double nCalc2 = rtime2 * speedMp;

	nCalc += 0.55 + size;
	nCalc2 += 0.55 + size * 1.5;

	lineData lndata;
	lndata.type = note.type;
	lndata.top = nCalc;
	lndata.bottom = nCalc2;

	player.tailsData.push_back(lndata);
}

void CGamePlay::renderNoteShadow(CPlayer::NotesData::Note &note, CPlayer &player)
{
	double time = /*time2Position(*/note.time/*)*/, ltimet = getRunningMusicTime(player);
	bool bAddTailToBuffer = false;
	double lt = 0.0, tlng = 0.0;
	for (int i = 0; i < 5; i++){
		if (note.type & /*(int)pow(2, i)*/notesFlagsConst[i])
		{
			unsigned int texture = /*fretsText.notesTexture*/1;
			/*if (note.type & notesFlags::nf_doing_slide){
			time = ltimet;
			}*/

			if (note.lTime > 0.0 && note.type & notesFlags::nf_doing_slide && !(note.type & notesFlags::losted))
			{
				bAddTailToBuffer = true;
				lt = ltimet;
				tlng = time - ltimet + note.lTime;
				//engine.bindVBOBuffer(0);
				//renderIndividualLine(i, ltimet, time - ltimet + note.lTime, GPPGame::GuitarPP().lineText, player);
			}
			else if (note.type & notesFlags::nf_slide && !(note.type & notesFlags::nf_slide_picked))
			{
				//engine.bindVBOBuffer(0);
				bAddTailToBuffer = true;
				lt = time;
				tlng = note.lTime;
				//renderIndividualLine(i, time, note.lTime, GPPGame::GuitarPP().lineText, player);
			}

			if ((note.type & notesFlags::nf_not_hopo) ^ notesFlags::nf_not_hopo)
			{
				texture = /*GPPGame::GuitarPP().HOPOSText*/-1;
			}

			if (!(note.type & notesFlags::nf_picked) && !(note.type & notesFlags::nf_doing_slide)) renderIndivdualNoteShadow(i, time, 0, bAddTailToBuffer, player);
		}
	}
}

void CGamePlay::renderNote(const CPlayer::NotesData::Note &note, CPlayer &player)
{
	double time = /*time2Position(*/note.time/*)*/, ltimet = getRunningMusicTime(player);
	double dif = time - ltimet;
	bool bAddTailToBuffer = false;
	double lt = 0.0, tlng = 0.0;

	if (note.type & notesFlags::nf_open)
	{
		if ((!(note.type & notesFlags::nf_picked) || dif > -0.5) && !(note.type & notesFlags::nf_doing_slide)) renderOpenNote(time, 0, note.type, player);
		return;
	}

	for (int i = 0; i < 5; i++)
	{
		if (note.type & /*(int)pow(2, i)*/notesFlagsConst[i])
		{
			unsigned int texture = /*fretsText.notesTexture*/1;

			if (note.lTime > 0.0 && note.type & notesFlags::nf_doing_slide && !(note.type & notesFlags::losted))
			{
				bAddTailToBuffer = true;
				lt = ltimet;
				tlng = time - ltimet + note.lTime;
			}
			else if (note.type & notesFlags::nf_slide && !(note.type & notesFlags::nf_slide_picked))
			{
				//engine.bindVBOBuffer(0);
				bAddTailToBuffer = true;
				lt = time;
				tlng = note.lTime;
				//renderIndividualLine(i, time, note.lTime, GPPGame::GuitarPP().lineText, player);
			}

			if ((note.type & notesFlags::nf_not_hopo) ^ notesFlags::nf_not_hopo)
			{
				texture = /*GPPGame::GuitarPP().HOPOSText*/-1;
			}

			if ((!(note.type & notesFlags::nf_picked) || dif > -0.5) && !(note.type & notesFlags::nf_doing_slide)) renderIndivdualNote(i, time, texture, note.type, player);
		}
	}

	if (bAddTailToBuffer) addTailToBuffer(note, lt, tlng, ltimet, player);
}

void CGamePlay::renderNotePlayer(CPlayer &player)
{
	auto &notes = player.Notes;
	auto &gNotes = player.Notes.gNotes;
    double musicTime = getRunningMusicTime(player);

    for (size_t i = notes.notePos, size = gNotes.size(); i < size; i++)
	{
		auto &note = gNotes[i];
		double noteTime = note.time - musicTime;
		double endNoteTime = noteTime + note.lTime;

		if (noteTime <= 5.0)
		{
			double endNoteTime = noteTime + note.lTime;

			bool bLongNPicked = (note.type & notesFlags::nf_slide_picked) != 0 && endNoteTime >= 0.03;
			if ((note.type & notesFlags::nf_picked) == 0 || noteTime >= -0.0025 || bLongNPicked)
			{
			    renderNote(note, player);
			}
		}
    }
}

void CGamePlay::renderNoteNoAdd(CPlayer::NotesData::Note &note, CPlayer &player)
{
	double time = /*time2Position(*/note.time/*)*/, ltimet = getRunningMusicTime(player);
	double dif = time - ltimet;
	double lt = 0.0, tlng = 0.0;
	for (int i = 0; i < 5; i++){
		if (note.type & /*(int)pow(2, i)*/notesFlagsConst[i])
		{
			unsigned int texture = /*fretsText.notesTexture*/1;
			/*if (note.type & notesFlags::nf_doing_slide){
			time = ltimet;
			}*/

			if ((!(note.type & notesFlags::nf_picked) || dif > -0.5) && !(note.type & notesFlags::nf_doing_slide)) renderIndivdualNote(i, time, texture, note.type, player);
		}
	}
}

void CGamePlay::updatePlayer(CPlayer &player, double deltatime)
{
	auto &notes = player.Notes;
	auto &gNotes = player.Notes.gNotes;
	auto &engine = CEngine::engine();

	int playerLVL = player.getLevel();

	if (player.plusEnabled)
	{
		player.plusPower -= deltatime * 0.05;

		if (player.plusPower <= 0.0)
		{
			player.plusEnabled = false;
			player.plusPower = 0.0;
		}
	}

	player.rangle = ((int)(CEngine::engine().getTime() * 400.0) % 360);

	player.update();

	//player.buffer.clear();

	double musicTime = getRunningMusicTime(player);

	double minendtime = 0.0;
	auto minendtimei = notes.notePos;
	//bool minendtimeslide = false;

	auto getHighestFlag = [](int flags)
	{
		int result = 0;
		for (int ji = 4; ji >= 0; ji--)
		{
			if (flags & /*(int)pow(2, ji)*/notesFlagsConst[ji])
			{
				result = /*(int)pow(2, ji)*/notesFlagsConst[ji];
				break;
			}
		}

		return result;
	};

	int fretsPressedFlags = player.getFretsPressedFlags();
	int lastFretsPressedFlags = player.getLastFretsPressedFlags();
	int highestFlagInPressedKey = getHighestFlag(fretsPressedFlags);
	int lastHighestFlagInPressedKey = getHighestFlag(lastFretsPressedFlags);
	
	bool noteDoedThisFrame = false;

	size_t plusSize = player.Notes.gPlus.size();

	if (player.Notes.plusPos < plusSize)
	{
		auto &plusp = player.Notes.gPlus[player.Notes.plusPos];

		int64_t tplusnotes = (plusp.lastNote - plusp.firstNote) + 1;

		if ((plusp.time + plusp.lTime) > (musicTime + 0.01) && plusp.time <= musicTime && tplusnotes > 0)
		{
			player.plusLoadF = ((double)(1 + notes.lastNotePicked - plusp.firstNote) / (double)tplusnotes);
		}
		else
		{
			player.plusLoadF = 0.0;
		}

		if ((plusp.time + plusp.lTime) < (musicTime + 0.01))
		{
			++player.Notes.plusPos;
		}
	}
	else
	{
		player.plusLoadF = player.plusLoadB = 0.0;
	}

	if (player.plusLoadF != player.plusLoadB)
	{
		player.plusLoadB += (player.plusLoadF - player.plusLoadB) * deltatime * 10.0;
	}

	bool inslide = false, inslide2 = false;

	if (gNotes.size() > notes.notePos)
	{
		for (int ji = 0; ji < 5; ji++)
		{
			if (gNotes[notes.notePos].type & /*(int)pow(2, ji)*/notesFlagsConst[ji])
			{
				const size_t npos = ~(static_cast<size_t>(0));

				if (player.notesSlide[ji] != npos)
				{
					inslide = true;
				}
			}
		}
	}

	if (!player.plusEnabled && player.plusPower > 0.0 && CEngine::engine().getKey(' '))
	{
		player.plusEnabled = true;
	}


	auto doPlus = [&](size_t i)
	{
		if (notes.plusPos < notes.gPlus.size())
		{
			auto &plPos = notes.gPlus[notes.plusPos];

			if (player.plusPower < player.maxPlusPower)
			{
				if (plPos.lastNote == i)
				{
					player.plusPower += player.maxPlusPower / 5.0;
					++notes.plusPos;
					player.plusThunterStrikeStart = engine.getTime();
					player.bPlusStrike = true;
				}
			}

			if (player.plusPower > player.maxPlusPower)
			{
				player.plusPower = player.maxPlusPower;
			}
		}
	};

	auto getLastNotePickedTimeDiff = [&](size_t i)
	{
		double result = 0;
		if (player.Notes.lastNotePicked == ~(static_cast<size_t>(0)))
		{
			return player.Notes.gNotes[i].time;
		}

		return player.Notes.gNotes[i].time - player.Notes.gNotes[player.Notes.lastNotePicked].time;
	};

	auto tapResulted = [&player](int flags)
	{
		double ttime = CEngine::engine().getTime();

		for (int ji = 0; ji < 5; ji++)
		{
			if (flags & notesFlagsConst[ji])
			{
				player.fretsPressedTime[ji] = ttime - 100.0;
			}
		}
	};

	auto doNoteFunc = [&](CPlayer::NotesData::Note &note, size_t i)
	{
		if (!noteDoedThisFrame && !bIsACharterGP)
		{
			tapResulted(note.type);

			if (note.lTime == 0.0)
			{
				player.doNote(i);
				note.type |= notesFlags::nf_picked;
				doPlus(i);

				noteDoedThisFrame = true;
				player.aError = false;

				{
					for (int ji = 0; ji < 5; ji++)
					{
						if (note.type & /*(int)pow(2, ji)*/notesFlagsConst[ji] || (note.type & notesFlags::nf_open) != 0)
						{
							notes.fretsNotePickedTime[ji] = engine.getTime();
							player.notesSlide[ji] = -1;
						}
					}
				}
			}
			else if (!(note.type & notesFlags::nf_doing_slide))
			{
				player.doNote(i);
				doPlus(i);

				noteDoedThisFrame = true;
				player.aError = false;

				{
					for (int ji = 0; ji < 5; ji++)
					{
						if (note.type & /*(int)pow(2, ji)*/notesFlagsConst[ji])
						{
							notes.fretsNotePickedTime[ji] = engine.getTime();
						}
					}
				}

				if (note.lTime > 0.0)
				{
					note.type |= notesFlags::nf_doing_slide;

					inslide2 = true;

					{
						for (int ji = 0; ji < 5; ji++)
						{
							if (note.type & /*(int)pow(2, ji)*/notesFlagsConst[ji])
							{
								player.notesSlide[ji] = i;
							}
						}
					}
				}
			}
		}
	};

	bool firstNoteToDoSetted = false;
	size_t firstNoteToDo = 0;
	bool strklinbotsetted = false;

	bool errorThisFrame = false, hopostrmm = false, strumdelayed = false;

	double BPS = 30.0 / player.Notes.BPM[player.BPMNowBuffer].lTime;

	auto checkCorrect = [&](int ntsInac, int ntsT, int noteHF, const CPlayer::NotesData::Note &note)
	{
		//if (/*player.palhetaKey &&*/ (note.type & notesFlags::nf_not_hopo))
		if(!bIsACharterGP) {
			if (ntsInac > 1 && fretsPressedFlags == ntsT)
			{
				return true;
			}
			else if (ntsInac == 1 && highestFlagInPressedKey == noteHF)
			{
				return true;
			}
			else if(ntsInac == 0 && fretsPressedFlags == 0)
			{
				return true;
			}
		}

		return false;
	};

	auto processError = [&]()
	{
		if (!bIsACharterGP)
		{
			player.processError();

			errorThisFrame = true;

			int fretPid = 0;
			for (auto &fretP : player.fretsPressed)
			{
				if (fretP || fretsPressedFlags == 0)
					CEngine::engine().playSoundStream(GPPGame::GuitarPP().errorsSound[fretPid]);

				fretPid++;
			}
		}
	};

	auto isTapping = [&player](int flags, double time)
	{
		bool result = true;

		double ttime = CEngine::engine().getTime();

		for (int ji = 0; ji < 5; ji++)
		{
			if (flags & notesFlagsConst[ji])
			{
				if (result)
				{
					result = (ttime - player.fretsPressedTime[ji]) < time;
				}
				else
				{
					break;
				}
			}
		}

		return result;
	};

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

		if (noteTime <= 5.0)
		{
			double endNoteTime = noteTime + note.lTime;

			bool bLongNPicked = (note.type & notesFlags::nf_slide_picked) != 0 && endNoteTime >= 0.03;
			if ((note.type & notesFlags::nf_picked) == 0 || noteTime >= -0.0025 || bLongNPicked)
			{
				//player.buffer.push_back(&note);
			}
		}

		bool tappable = note.type & notesFlags::noteTap;

		if (endNoteTime > -1.5 && noteTime < 5.0)
		{
			if (!hopostrmm)
			{
				if (noteTime >= -0.05 && noteTime < 0.1 && !(note.type & notesFlags::hopontstrmmd) && highestFlagInPressedKey == getHighestFlag(note.type & player.notesEnum) && player.palhetaKey)
				{
					hopostrmm = true;
					note.type |= notesFlags::hopontstrmmd;
				}
			}

			const bool isStrum = (note.type & notesFlags::nf_not_hopo) != 0;

			int ntsInac = 0;
			const int ntsT = note.type & CPlayer::notesEnum;
			const int noteHF = getHighestFlag(ntsT);

			for (int i = 0; i < 5; ++i)
			{
				if (note.type & /*(int)pow(2, i)*/notesFlagsConst[i])
				{
					ntsInac++;
				}
			}

			if (!noteDoedThisFrame && noteTime >= 0.1 && noteTime < 0.15 && !(note.type & notesFlags::nf_picked) && !(note.type & notesFlags::strmstlrc))
			{
				if (player.palhetaKey && (note.type & notesFlags::nf_not_hopo) && (ntsT != 0 || player.usarPalheta))
				{
					if (checkCorrect(ntsInac, ntsT, noteHF, note))
					{
						note.type |= notesFlags::strmstlrc;
						strumdelayed = true;
					}
				}
			}


			if (!noteDoedThisFrame && noteTime > -0.05 && noteTime < 0.15 && !strklinbotsetted)
			{
				player.strklinent = i;
				player.strklinenttime = noteTime;
				strklinbotsetted = true;
			}

			if (noteTime > -0.05 && noteTime < 0.1 && !(note.type & notesFlags::nf_picked) && !(note.type & notesFlags::nf_failed))
			{
				if (!noteDoedThisFrame)
				{
					bool palhetaTest = player.palhetaKey;

					if (!player.usarPalheta)
					{
						palhetaTest = isTapping(note.type, 0.15 - noteTime);
					}

					if (isStrum)
					{
						if (player.palhetaKey)
						{
							if (note.type & notesFlags::strmstlrc)
							{
								if (checkCorrect(ntsInac, ntsT, noteHF, note))
								{
									doNoteFunc(note, i);
									player.lastHOPO = 0;
								}
								else
								{
									////////////////////////////////////////////////////////////////
									processError();
									////////////////////////////////////////////////////////////////
								}
							}
							else if (ntsInac > 1 && fretsPressedFlags == ntsT)
							{
								doNoteFunc(note, i);
								player.lastHOPO = 0;
							}
							else if (ntsInac == 1 && highestFlagInPressedKey == noteHF)
							{
								doNoteFunc(note, i);
								player.lastHOPO = noteHF;
							}
							else if (ntsInac == 0 && fretsPressedFlags == 0)
							{
								doNoteFunc(note, i);
								player.lastHOPO = 0;
							}
							else {
								////////////////////////////////////////////////////////////////
								processError();
								////////////////////////////////////////////////////////////////
							}
						}
						else if (!player.usarPalheta)
						{
							if (!player.aError && (note.type & notesFlags::nf_open) && ntsInac == 0 && fretsPressedFlags == 0)
							{
								doNoteFunc(note, i);
								player.lastHOPO = 0;
							}
						}
					}
					else if (!(note.type & notesFlags::nf_not_hopo))
					{
						//int noteHF = getHighestFlag(note.type & player.notesEnum);
						if (highestFlagInPressedKey == noteHF)
						{
							bool regiserr = false;

							if (firstNoteToDoSetted && firstNoteToDo != i)
							{
								regiserr = true;
							}

							if (!player.aError && !regiserr)
							{
								if (highestFlagInPressedKey != lastHighestFlagInPressedKey)
								{
									doNoteFunc(note, i);
									player.lastHOPO = noteHF;
								}
								else if (getLastNotePickedTimeDiff(i) <= BPS && player.lastHOPO != noteHF)
								{
									doNoteFunc(note, i);
									player.lastHOPO = noteHF;
								}
							}
							else
							{
								if (palhetaTest && (highestFlagInPressedKey != 0 || player.palhetaKey))
								{
									// TODO: Add some distortion
									player.processErrorNonPickedB(i);

									doNoteFunc(note, i);
									regiserr = false;
									player.lastHOPO = noteHF;
								}
							}

							if (regiserr)
							{
								player.aError = true;
							}
						}
					}

					if ((tappable || !player.usarPalheta) && isStrum && !noteDoedThisFrame && !errorThisFrame && isTapping(note.type, 0.15 - noteTime))
					{
						if (note.type & notesFlags::strmstlrc)
						{
							if (checkCorrect(ntsInac, ntsT, noteHF, note))
							{
								doNoteFunc(note, i);
								player.lastHOPO = 0;
							}
							else
							{
								////////////////////////////////////////////////////////////////
								//processError();
								////////////////////////////////////////////////////////////////
							}
						}
						else if (ntsInac > 1 && fretsPressedFlags == ntsT)
						{
							doNoteFunc(note, i);
							player.lastHOPO = 0;
						}
						else if (ntsInac == 1 && highestFlagInPressedKey == noteHF)
						{
							doNoteFunc(note, i);
							player.lastHOPO = noteHF;
						}
					}
				}
			}
			else if (noteTime <= -0.05 && !bIsACharterGP)
			{
				if (!(note.type & notesFlags::nf_failed) && !(note.type & notesFlags::nf_picked) && !(note.type & notesFlags::nf_doing_slide))
				{
					note.type |= notesFlags::nf_failed;
					player.processError();
					player.aError = true;
				}
			}
			else
			{
				if (!firstNoteToDoSetted && !bIsACharterGP)
				{
					if (!(note.type & notesFlags::nf_failed) && !(note.type & notesFlags::nf_picked) && !(note.type & notesFlags::nf_doing_slide))
					{
						firstNoteToDo = i;
						firstNoteToDoSetted = true;
					}
				}
			}
			

			if (endNoteTime < 0.0 && note.type & notesFlags::nf_slide && !bIsACharterGP)
			{
				note.type |= notesFlags::nf_picked;
				note.type |= notesFlags::nf_slide_picked;
			}



			/*if (noteTime > -0.1 && noteTime < 0.05 && !(note.type & notesFlags::nf_picked))
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
					doPlus(i);
				}
				else if (!(note.type & notesFlags::nf_doing_slide))
				{
					player.doNote(i);
					doPlus(i);
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
			}*/
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

	bool fretpError = false;

	if (!bIsACharterGP)
	{
		if (!errorThisFrame && !noteDoedThisFrame && player.palhetaKey && !hopostrmm && !strumdelayed)
		{
			int fretPid = 0;
			for (auto &fretP : player.fretsPressed)
			{
				if (fretP || fretsPressedFlags == 0)
					CEngine::engine().playSoundStream(GPPGame::GuitarPP().errorsSound[fretPid]);

				fretPid++;
			}

			fretpError = true;
			player.processError();
		}

		bool cklngNote = false;

		{
			bool cancelAllLongNotes = false;

			for (int ji = 0; ji < 5; ji++)
			{
				size_t id = player.notesSlide[ji];

				if (id != -1)
				{
					cklngNote = true;
					auto &note = player.Notes.gNotes[id];
					double noteTime = note.time - musicTime;
					double endNoteTime = noteTime + note.lTime;

					int ntsInac = 0;
					int ntsT = note.type & CPlayer::notesEnum;

					for (int i = 0; i < 5; ++i)
					{
						if (note.type & /*(int)pow(2, i)*/notesFlagsConst[i])
						{
							ntsInac++;
						}
					}

					if (fretsPressedFlags != ntsT)
					{
						if (ntsInac > 1){
							cancelAllLongNotes = true;
						}
						/////// TODO extra checks
						else if (highestFlagInPressedKey != getHighestFlag(ntsT))
						{
							cancelAllLongNotes = true;
						}
					}

					if (endNoteTime < 0.13)
					{
						player.notesSlide[ji] = -1;
						note.type |= notesFlags::nf_picked;
						note.type |= notesFlags::nf_slide_picked;
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

			if (cancelAllLongNotes || fretpError)
			{
				cklngNote = false;
				player.muteInstrument();
				for (int ji = 0; ji < 5; ji++)
				{
					size_t id = player.notesSlide[ji];

					if (id != -1)
					{
						auto &note = player.Notes.gNotes[id];

						player.notesSlide[ji] = -1;
						note.type |= notesFlags::losted;
					}

					player.notesSlide[ji] = -1;
					inslide2 = false;
				}
			}
		}



		if (cklngNote)
		{
			player.addPointsByDoingLongNote();
		}

		if (noteDoedThisFrame && GPPGame::GuitarPP().showTextsTest)
		{
			int nowLevel = player.getLevel();
			if (nowLevel != playerLVL)
			{
				CFonts::textAlert t;

				t.callback = alertTest;
				t.msg = "Level Up! " + std::to_string(nowLevel);
				t.startTime = engine.getTime();

				CFonts::fonts().addTextAlert(std::move(t));
			}

			int64_t combo = player.getCombo();

			if (combo > 0)
			{
				if (combo == 50)
				{
					CFonts::textAlert t;

					t.callback = alertTest;
					t.msg = "50 Note Streak";
					t.startTime = engine.getTime();

					CFonts::fonts().addTextAlert(std::move(t));
				}
				else
				{
					int r = combo % 100;

					if (r == 0)
					{
						CFonts::textAlert t;

						t.callback = alertTest;
						t.msg = std::to_string(combo) + " Note Streak";
						t.startTime = engine.getTime();

						CFonts::fonts().addTextAlert(std::move(t));
					}
				}
			}
		}
	}

}

void CGamePlay::renderFretBoardShadow(CPlayer &player, double x1, double x2, double x3, double x4, unsigned int Text)
{
	fretboardLData.clear();
	fretboardLData.useColors = true;
	fretboardLData.autoEnDisaColors = true;

	double size = 2.1;

	auto positionCalcByT = [this, size](double p, double prop)
	{
		double cCalc = -p * 5.0;
		double propSpeeed = 5.0 / speedMp;

		cCalc /= propSpeeed;

		cCalc *= 1000000.0;
		cCalc = static_cast<double>((int64_t)cCalc % (int64_t)((size * 1000000) * prop));
		return cCalc / 1000000.0;
	};

	double musicRunningTime = getRunningMusicTime(player) - 4.0;
	double cCalc = positionCalcByT(musicRunningTime, (x2 - x1));

	CEngine::RenderDoubleStruct FretBoardStruct;

	FretBoardStruct.x1 = x1;
	FretBoardStruct.x2 = x2;
	FretBoardStruct.x3 = x3;
	FretBoardStruct.x4 = x4;

	FretBoardStruct.TextureX1 = 0.0;
	FretBoardStruct.TextureX2 = 1.0;

	FretBoardStruct.TextureY1 = 0.0;
	FretBoardStruct.TextureY2 = 1.0;

	FretBoardStruct.y1 = -0.5;
	FretBoardStruct.y2 = FretBoardStruct.y1;
	FretBoardStruct.y3 = FretBoardStruct.y1;
	FretBoardStruct.y4 = FretBoardStruct.y1;

	fretboardLData.texture = FretBoardStruct.Text = 0;

	for (int i = -2; i < 10; i++)
	{
		FretBoardStruct.z1 = (x2 - x1) * (-size) * i - cCalc;
		FretBoardStruct.z2 = FretBoardStruct.z1;
		FretBoardStruct.z3 = FretBoardStruct.z1 + (x2 - x1) * (-size);
		FretBoardStruct.z4 = FretBoardStruct.z3;

		FretBoardStruct.alphaBottom = pos2Alpha(-FretBoardStruct.z3 / 5.5);
		FretBoardStruct.alphaTop = pos2Alpha(-FretBoardStruct.z2 / 5.5);

		if (FretBoardStruct.alphaBottom <= 0.0 && FretBoardStruct.alphaTop <= 0.0)
		{
			continue;
		}

		if (FretBoardStruct.alphaBottom < 0.0)
		{
			FretBoardStruct.alphaBottom = 0.0;
		}

		if (FretBoardStruct.alphaTop < 0.0)
		{
			FretBoardStruct.alphaTop = 0.0;
		}

		//CFonts::fonts().draw3DTextInScreen("TESTE", CFonts::fonts().getCenterPos(sizeof("TESTE") - 1, 0.2, x1 + (x2 - x1) / 2.0), -0.4992, FretBoardStruct.z1, 0.2, 0.0, -0.2);

		CEngine::pushQuad(fretboardLData, FretBoardStruct);
	}

	CEngine::engine().drawTrianglesWithAlpha(fretboardLData);
}

void CGamePlay::renderFretBoard(CPlayer &player, double x1, double x2, double x3, double x4, unsigned int Text)
{
	fretboardLData.clear();
	fretboardLData.useColors = true;
	fretboardLData.autoEnDisaColors = false;

	const double size = 2.1 * 1.33333333;

	auto positionCalcByT = [this, size](double p, double prop)
	{
		double cCalc = -p * 5.0;
		double propSpeeed = 5.0 / speedMp;

		cCalc /= propSpeeed;

		cCalc *= 1000000.0;
		cCalc = static_cast<double>((int64_t)cCalc % (int64_t)((size * 1000000) * prop));
		return cCalc / 1000000.0;
	};
	
	double musicRunningTime = getRunningMusicTime(player) - 4.0;
	double cCalc = positionCalcByT(musicRunningTime, (x2 - x1));

	CEngine::RenderDoubleStruct FretBoardStruct;

	FretBoardStruct.x1 = x1;
	FretBoardStruct.x2 = x2;
	FretBoardStruct.x3 = x3;
	FretBoardStruct.x4 = x4;

	FretBoardStruct.TextureX1 = 0.0;
	FretBoardStruct.TextureX2 = 1.0;

	FretBoardStruct.TextureY1 = 0.0;
	FretBoardStruct.TextureY2 = 1.0;

	FretBoardStruct.y1 = -0.5;
	FretBoardStruct.y2 = FretBoardStruct.y1;
	FretBoardStruct.y3 = FretBoardStruct.y1;
	FretBoardStruct.y4 = FretBoardStruct.y1;

	fretboardLData.texture = FretBoardStruct.Text = Text;

	const double x2mx1nsize = (x2 - x1) * (-size);

	for (int i = -2; i < 12; i++)
	{
		FretBoardStruct.z1 = x2mx1nsize * i - cCalc;
		FretBoardStruct.z2 = FretBoardStruct.z1;
		FretBoardStruct.z3 = FretBoardStruct.z1 + x2mx1nsize;
		FretBoardStruct.z4 = FretBoardStruct.z3;

		FretBoardStruct.alphaBottom = pos2Alpha(-FretBoardStruct.z3 / 5.5);
		FretBoardStruct.alphaTop = pos2Alpha(-FretBoardStruct.z2 / 5.5);

		bool cont = false;

		if (FretBoardStruct.alphaBottom <= 0.0)
		{
			FretBoardStruct.alphaBottom = 0.0;

			FretBoardStruct.z3 = 1.6 * -5.5;
			FretBoardStruct.z4 = FretBoardStruct.z3;

			FretBoardStruct.TextureY2 = ((FretBoardStruct.z3 - FretBoardStruct.z1) / x2mx1nsize);
			cont = true;
		}

		CEngine::pushQuad(fretboardLData, FretBoardStruct);

		if (cont)
		{
			break;
		}
	}

	CEngine::engine().drawTrianglesWithAlpha(fretboardLData);

}

void CGamePlay::renderPlayerPylmBar(CPlayer &player)
{
	CEngine::engine().renderAt(0.0, -0.5, 1.1);

	player.guitar->gameplayBar.draw(player.guitar->textureID, false);
}

void CGamePlay::renderPylmBar()
{
	CEngine::engine().renderAt(0.0, -0.5, 1.1);

	GPPGame::GuitarPP().pylmbarOBJ.draw(GPPGame::GuitarPP().pylmBarText, false);
}

void CGamePlay::renderLyrics()
{
	if (songlyrics.size() > songlyricsIndex)
	{
		double time = getPlayer(0).musicRunningTime;

		if (time > songlyrics[songlyricsIndex].tend)
		{
			++songlyricsIndex;
			return;
		}

		if (time > songlyrics[songlyricsIndex].tstart)
		{
			std::string &s = songlyrics[songlyricsIndex].lyric;
			CFonts::fonts().drawTextInScreenWithBuffer(s, CFonts::fonts().getCenterPos(s, 0.1, 0.0), 0.6, 0.1);
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

	std::cout << lyricFile << " " << lyrics.is_open() << std::endl;

	if (lyrics.is_open())
	{
		char temp[1024] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
		char temp2[128] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };

		int s = 0;
		int id = 0, result = 0;
		int sinc[8] = {0, 0, 0, 0, 0, 0, 0, 0};

		double offset = 0.0, offt;

		//lyrics.seekg(0, std::ios::end);
		//std::cout << lyrics.tellg() << std::endl;
		lyrics.seekg(0, std::ios::beg);

		lyricLine l;

		while (lyrics.getline(temp, sizeof(temp)))
		{
			if (strlen(temp) == 0)
			{
				continue;
			}

			result = sscanf(temp, "%127s %lf", temp2, &offt);

			if (result == 2 && std::string(temp2) == "offset")
			{
				offset = offt;

				std::cout << "offset " << offt << std::endl;
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

void CGamePlay::renderNoteShadowHpStyle(CPlayer &player)
{
	/*double size = 0.2;
	double position = -0.51;

	CEngine::RenderDoubleStruct TempStruct3D;

	TempStruct3D.Text = GPPGame::GuitarPP().loadTexture("data/sprites", "shadow.tga").getTextId();
	TempStruct3D.TextureX1 = 0.0;
	TempStruct3D.TextureX2 = 1.0;
	TempStruct3D.TextureY1 = 1.0;
	TempStruct3D.TextureY2 = 0.0;

	double time = getRunningMusicTime(player);

	//CFonts::fonts().drawTextInScreen(std::to_string(hopostp.size()), 0.0, 0.5, 0.1);

	for (auto it = player.buffer.rbegin(); it != player.buffer.rend(); it++)
	{
		const auto &note = **it;

		double rtime = time - note.time;
		for (int i = 0; i < 5; ++i)
		{
			if (note.type & notesFlagsConst[i])
			{
				double x = 0, y = -0.42, z = 0;

				{
					double nCalc = rtime * speedMp;

					nCalc += 0.55;

					x = position + (double(i) * size / 48.0) + (double(i) * size);
					z = nCalc;
				}

				TempStruct3D.x1 = x;
				TempStruct3D.x2 = TempStruct3D.x1 + size;
				TempStruct3D.x3 = TempStruct3D.x1 + size;
				TempStruct3D.x4 = TempStruct3D.x1;

				TempStruct3D.y1 = y;
				TempStruct3D.y2 = TempStruct3D.y1;
				TempStruct3D.y3 = y;
				TempStruct3D.y4 = TempStruct3D.y3;

				TempStruct3D.z1 = z + size;
				TempStruct3D.z2 = TempStruct3D.z1;
				TempStruct3D.z3 = z + size * 2.0;
				TempStruct3D.z4 = TempStruct3D.z3;

				CEngine::engine().setColor(1.0, 1.0, 1.0, 1.0);

				CEngine::engine().Render3DQuad(TempStruct3D);
			}
		}
	}


	CEngine::engine().setColor(1.0, 1.0, 1.0, 1.0);
	hopostp.clear();*/
}

void CGamePlay::setHyperSpeed(double s)
{
	speedMp = s;
}

void CGamePlay::setMusicSpeed(double s)
{
	gSpeed = s;
}

void shadowMatrix(float m[4][4],
	float plane[4],
	float light[4])
{
	float dot = plane[0] * light[0] + plane[1] * light[1] +
		plane[2] * light[2] + plane[3] * light[3];
	m[0][0] = dot - light[0] * plane[0];
	m[1][0] = -light[0] * plane[1];
	m[2][0] = -light[0] * plane[2];
	m[3][0] = -light[0] * plane[3];
	m[0][1] = -light[1] * plane[0];
	m[1][1] = dot - light[1] * plane[1];
	m[2][1] = -light[1] * plane[2];
	m[3][1] = -light[1] * plane[3];
	m[0][2] = -light[2] * plane[0];
	m[1][2] = -light[2] * plane[1];
	m[2][2] = dot - light[2] * plane[2];
	m[3][2] = -light[2] * plane[3];
	m[0][3] = -light[3] * plane[0];
	m[1][3] = -light[3] * plane[1];
	m[2][3] = -light[3] * plane[2];
	m[3][3] = dot - light[3] * plane[3];
}

void CGamePlay::renderPlayer(CPlayer &player)
{
	player.tailsData.clear();
	lightData l0;

	auto &engine = CEngine::engine();

	/*static OutputSurface_t surf = []() { OutputSurface_t result;
	memset(&result, 0, sizeof(result)); return result; }();
	
	bool b = ([&]() {
	//surf.surface = nullptr;

	static int vframe = 0;

	//result = minivideo_extract(aaaaaa.getContext(), "./aaaa", false, true, false);
	//MessageBoxA(0, (std::string("decode fail ") + std::to_string((uintptr_t)result)).c_str(), "aaaa", 0);

	{
		static double time = engine.getTime() - 1.2;
		double tnow = engine.getTime();
		if ((tnow - time) > 1.0)
		{
			time = tnow;
			int result = minivideo_decode(aaaaaa.getContext(), &surf, vframe);
			//vframe++;
		}
	}

	return true;
	})();

	std::cout << surf.width << "  " << surf.height << "  " << (uintptr_t)surf.surface  << std::endl;

	if (surf.surface)
	{
		glDrawPixels(surf.width, surf.height, GL_RGB, GL_UNSIGNED_BYTE, surf.surface);
	}*/

	/*static double ffatime = engine.getTime();
	double p = pow(10, 30);

	if (engine.getTime() - ffatime > 0.02)
	{
		auto ch = engine.getChannelData(player.instrumentSound, 1);
		ffatime = engine.getTime();

		for (int i = 0; i < 4; i++)
		{
			player.spectrumLines[i * 2 + 0] = -0.6 + (double)i * 0.3;
			//////*
			std::string test = std::to_string(ch);
			if (ch.data[i] > 0)
				test.resize(10, '0');
			else
				test.resize(11, '0');

			double valueresult = std::stod(test) / 1000000000.0;
			valueresult /= 64.0;
			///////////
			player.spectrumLines[i * 2 + 1] = ch.data[i] + 0.5;

			//std::cout << valueresult << std::endl;
		}
	}

	engine.draw2DLine(player.spectrumLines, 4);
	*/
	if (bFretboardLightFading)
	{
		if (fretboardLightFade > 1.0)
		{
			fretboardLightFade -= engine.getDeltaTime() * 4.0;
		}
		else
		{
			bFretboardLightFading = false;
		}
	}

	for (auto &t : l0.ambientLight)
	{
		t = 0.2f;

		if (bFretboardLightFading)
		{
			t /= fretboardLightFade;
		}
	}

	for (auto &t : l0.direction)
	{
		t = 2.5f;
	}

	for (auto &t : l0.position)
	{
		t = 0.0f;
	}

	for (auto &t : l0.specularLight)
	{
		t = 1.0f;
	}

	for (auto &t : l0.diffuseLight)
	{
		t = 1.0f;
	}

	l0.specularLight[3] = 1.0f;
	l0.diffuseLight[3] = 0.3f;

	l0.angle = 120.0f;
	l0.direction[0] = 0.0f;
	l0.direction[1] = -0.5f;
	l0.direction[2] = -5.0f;

	l0.position[3] = 1.0f;
	l0.position[1] = 0.0f;
	l0.position[2] = 2.5f;

	if (bFretboardLightFading)
	{
		l0.position[2] -= fretboardLightFade - 1.0;
	}

	auto &lua = CLuaH::Lua();

	bool renablel0 = false;

	if (player.bPlusStrike)
	{
		double difftime = engine.getTime() - player.plusThunterStrikeStart;
		
		if (difftime > 4.0)
		{
			player.bPlusStrike = false;
		}
		
		if (difftime > 0.0)
		{
			difftime /= 4.0;

			engine.activateLighting(true);

			lightData l;

			for (auto &t : l.ambientLight)
			{
				t = static_cast<float>(difftime);
			}

			for (auto &t : l.direction)
			{
				t = 2.5f;
			}

			for (auto &t : l.position)
			{
				t = 0.0f;
			}

			for (auto &t : l.specularLight)
			{
				t = 1.0f;
			}

			for (auto &t : l.diffuseLight)
			{
				t = 0.2f;
			}

			l.specularLight[2] = 1.0f;
			l.specularLight[3] = 1.0f - static_cast<float>(difftime);
			l.diffuseLight[2] = 1.0f;
			l.diffuseLight[3] = 1.0f - static_cast<float>(difftime);
			l.ambientLight[3] = static_cast<float>(difftime);

			l.angle = 100.0f;
			l.direction[0] = 0.0f;
			l.direction[1] = -0.5f;
			l.direction[2] = -5.0f;

			l.position[3] = 1.0f;
			l.position[1] = 0.0f;
			l.position[2] = 2.5f;

			engine.activateLight(0, false);
			engine.activateLight(1, true);
			engine.setLight(l, 1);
			renablel0 = true;
		}
	}

	if (player.plusEnabled)
	{
		engine.activateLighting(true);

		lightData l;

		for (auto &t : l.ambientLight)
		{
			t = 0.1f;
		}

		for (auto &t : l.direction)
		{
			t = 2.5f;
		}

		for (auto &t : l.position)
		{
			t = 0.0f;
		}

		for (auto &t : l.specularLight)
		{
			t = 1.0f;
		}

		for (auto &t : l.diffuseLight)
		{
			t = 0.2f;
		}

		l.specularLight[2] = 1.0f;
		l.specularLight[3] = 1.0f;
		l.diffuseLight[2] = 1.0f;
		l.diffuseLight[3] = 1.0f;
		l.ambientLight[3] = 0.1f;

		l.angle = 100.0f;
		l.direction[0] = 0.0f;
		l.direction[1] = -0.5f;
		l.direction[2] = -5.0f;

		l.position[3] = 1.0f;
		l.position[1] = 0.0f;
		l.position[2] = 2.5f;

		engine.activateLight(0, false);
		engine.activateLight(1, true);
		engine.setLight(l, 1);
	}

	CLuaH::multiCallBackParams_t m;

	m.push_back(player.plname);

	lua.runEventWithParams(preRenderPlayerSEvent, m);

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

		engine.setCamera(player.playerCamera);
	}

	/*{

		CEngine::cameraSET usingCamera;
		usingCamera.eyex = l0.position[0];
		usingCamera.eyey = l0.position[1];
		usingCamera.eyez = l0.position[2];
		usingCamera.centerx = l0.direction[0];
		usingCamera.centery = l0.direction[1];
		usingCamera.centerz = l0.direction[2];
		usingCamera.upx = 0;
		usingCamera.upy = 1;
		usingCamera.upz = 0;

		engine.setCamera(usingCamera);
	}*/


	engine.activate3DRender(true);

	double fretboardData[] = { -0.51, 0.51, 0.51, -0.51, -1.0, -1.0, 0.4, 0.4 };
	//engine.clearAccmumaltionBuffer();
	//engine.clearAccmumaltionBuffer();
	//player.playerParticles.render();
	//	engine.addToAccumulationBuffer(0.25);
	//engine.retAccumulationBuffer(1.0);
	CEngine::enableColorsPointer(true);

	//engine.setColor(1.0, 1.0, 1.0, 1.0);
	if (bFretboardLightFading)
	{
		engine.activateLighting(true);
		engine.activateLight(1, false);
		engine.activateLight(0, true);

		engine.setLight(l0, 0);
	}

	unsigned int fretboardText = (player.guitar == nullptr)? GPPGame::GuitarPP().fretboardText :
								((player.guitar->fretboardText != 0)? player.guitar->fretboardText : GPPGame::GuitarPP().fretboardText);

	renderFretBoard(player, fretboardData[0], fretboardData[1], fretboardData[2], fretboardData[3], fretboardText);
	//engine.addToAccumulationBuffer(0.5);

	if (showBPMLines) drawBPMLines(player);

	static float floorVertices[4][3] = {
		{ -20.0, -0.5, 20.0 },
		{ 20.0, -0.5, 20.0 },
		{ 20.0, -0.5, -20.0 },
		{ -20.0, -0.5, -20.0 },
	};

	auto renderScene = [&]()
	{
		engine.setColor(1.0, 1.0, 1.0, 1.0);
		if (player.guitar == nullptr)
		{
			renderPylmBar();
		}
		else
		{
			renderPlayerPylmBar(player);
		}

		//engine.clearAccmumaltionBuffer();

		/*for (auto it = player.buffer.rbegin(); it != player.buffer.rend(); it++)
		{
			const auto &n = **it;

			renderNote(n, player);
			//renderNoteShadow(n, player);
		}*/

        renderNotePlayer(player);
	};

	if (!bFretboardLightFading)
	{
		engine.activateLighting(true);


		//engine.setColor(1.0, 1.0, 1.0, 1.0);
		//if (player.plusEnabled || renablel0)
		{
			engine.activateLight(1, false);
			engine.activateLight(0, true);

			engine.setLight(l0, 0);
		}
	}

	static float floorShadow[4][4];

	float lightPosition[4] = { 0, 0, 2.5, 1.0 };
    
	CEngine::enableColorsPointer(false);

	engine.activateNormals(true);

	renderScene();

	engine.setColor(1.0, 1.0, 1.0, 1.0);

	for (int i = 0; i < 5; i++)
		renderIndivdualStrikeButton3D(i, 0.0, 0, 0.0, player);

	//engine.bindVBOBuffer(0);

	for (int i = 0; i < 5; i++)
	{
		double time = engine.getTime();
		double pressT = (time - player.Notes.fretsNotePickedTime[i]) /*/ 1.25*/;
		double calcP = (sin(pressT / 0.05)) / 15.0 - 0.05;

		if (pressT > 0.15 && player.notesSlide[i] == -1)
		{
			calcP = -10.0;

			if (player.fretsPressed[i])
			{
				calcP = -0.025;
			}
		}

		if (calcP < -0.035)
		{
			calcP = -0.035;
		}
		else if (calcP > -0.023)
		{
			int calcTest = (int(time * 1000.0) % 50);

			if (player.plusLoadF > 0.0)
			{
				calcTest %= 5;
			}
			else
			{
				calcTest %= 2;
			}

			if (calcTest)
			{
				CParticle::particleData pd;

				pd.duration = 0.04;

				double poscalc = -0.40 + (double(i) * 0.2 / 48.0) + (double(i) * 0.2);

				pd.x = poscalc + (rand() % 20 / 500.0) * -(rand() % 2);
				pd.y = -0.51;
				pd.z = 0.76 + (rand() % 20 / 500.0) * -(rand() % 2);

				pd.ax = 100.0 * -(rand() % 2);
				pd.ay = 100.0;
				pd.az = 200.0 * -(rand() % 2);

				pd.sx = 0.1;
				pd.sy = 0.1;
				pd.sz = 0.1;

				pd.sizex = 0.01;
				pd.sizey = 0.01;
				pd.sizez = 0.00;

				pd.desac = 0.2;

				pd.texture = pfireText;

				pd.qtd = 1;

				player.playerParticles.addParticle(pd);
			}
		}

		renderIndivdualStrikeButton3DStrike(i, 0.0, 0, calcP, player);
	}

	engine.bindVBOBuffer(0);

	engine.activateNormals(false);
	CEngine::engine().matrixReset();

	engine.activateLighting(false);

	if (player.playerParticles.part.size())
	{
		//engine.clearAccmumaltionBuffer();
		player.playerParticles.render();
		//	engine.addToAccumulationBuffer(0.25);
		//engine.retAccumulationBuffer(1.0);
	}

	engine.setColor(1.0, 1.0, 1.0, 1.0);

	renderTailsBuffer(player);
	engine.activate3DRender(false);

	renderHoposLight();


	{
		CEngine::cameraSET usingCamera;
		usingCamera.eye.x = 0.0;
		usingCamera.eye.y = 0.0;
		usingCamera.eye.z = 2.3;
		usingCamera.center.x = 0.0;
		usingCamera.center.y = 0.0;
		usingCamera.center.z = 0.0;
		usingCamera.up.x = 0.0;
		usingCamera.up.y = 1.0;
		usingCamera.up.z = 0.0;

		engine.setCamera(usingCamera);
	}

	if (bRenderHUD){

		CEngine::RenderDoubleStruct HUDBackground;

		HUDBackground.Text = GPPGame::GuitarPP().HUDText;

		double neg = 0.1 + player.playerHudOffsetX, negy = 0.05 + player.playerHudOffsetY;

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
		double circleMultiPercent = multi >= 4.0 ? 1.0 : (multi - floor(multi));
		double circlePublicAprov = (player.publicAprov / player.maxPublicAprov);
		double circleLoadPercent = player.plusLoadB;
		double circlePercent = (player.plusPower / player.maxPlusPower);
        double correctNotes = 0.0;
        size_t gNotesSize = 0;

        {
            correctNotes = player.correctNotes;
            gNotesSize = player.Notes.gNotes.size();
        }

		engine.setColor(1.0, 1.0, 1.0, 1.0);

		if (circleMultiPercent > 0.0)
        {
			double zeroToOne = circleMultiPercent;

			engine.setColor(0.0, 0.4, 1.0, 1.0);
			engine.Render2DCircleBufferMax(-0.8 + neg, -0.31 + negy, circleMultiPercent, 0.01, 0.041, 200, player.multiplierBuffer);
		}

		if (gNotesSize > 0)
        {
			double musicTotalCorrect = (correctNotes / gNotesSize);

			if (musicTotalCorrect > 0.0)
			{
				engine.setColor(0.4, 1.0, 0.4, 1.0);
				engine.Render2DCircleBufferMax(-0.8 + neg, -0.31 + negy, musicTotalCorrect, 0.05, 0.041, 400, player.correctNotesBuffer);
			}
		}

		if (circlePublicAprov > 0.0)
        {
			double zeroToOne = circlePublicAprov;

			engine.setColor(1.0 - 1.0 * zeroToOne, 1.0 * zeroToOne, 0.0, 1.0);
			engine.Render2DCircleBufferMax(-0.8 + neg, -0.31 + negy, circlePublicAprov, 0.09, 0.041, 600, player.publicApprovBuffer);
		}

		if (circleLoadPercent > circlePercent){
			if (circleLoadPercent > 0.0){
				double zeroToOne = circleLoadPercent;

				engine.setColor(0.4, 1.0, 0.4, 1.0);
				engine.Render2DCircleBufferMax(-0.8 + neg, -0.31 + negy, circleLoadPercent, 0.13, 0.04, 1000, player.plusLoadBuffer);
			}

			if (circlePercent > 0.0){
				double zeroToOne = circlePercent;

				engine.setColor(0.0, 1.0, 1.0, 1.0);
				engine.Render2DCircleBufferMax(-0.8 + neg, -0.31 + negy, circlePercent, 0.13, 0.04, 1000, player.plusCircleBuffer);
			}
		}
		else{
			if (circlePercent > 0.0){
				double zeroToOne = circlePercent;

				engine.setColor(0.0, 1.0, 1.0, 1.0);
				engine.Render2DCircleBufferMax(-0.8 + neg, -0.31 + negy, circlePercent, 0.13, 0.04, 1000, player.plusCircleBuffer);
			}

			if (circleLoadPercent > 0.0){
				double zeroToOne = circleLoadPercent;

				engine.setColor(0.4, 1.0, 0.4, 1.0);
				engine.Render2DCircleBufferMax(-0.8 + neg, -0.31 + negy, circleLoadPercent, 0.13, 0.04, 1000, player.plusLoadBuffer);
			}
		}

		engine.setColor(1.0, 1.0, 1.0, 1.0);

		//CFonts::fonts().drawTextInScreen(std::to_string(player.plusLoadB), 0.0, 0.2, 0.1);

		CFonts::fonts().drawTextInScreenWithBuffer(std::to_string(player.getCombo()), -0.974 + neg, 0.04 + negy, 0.1);
		CFonts::fonts().drawTextInScreenWithBuffer(std::to_string(player.getPoints()), -0.99 + neg, -0.1 + negy, 0.06);
		CFonts::fonts().drawTextInScreenWithBuffer(std::to_string((int)player.comboToMultiplier()), -0.85 + neg, -0.37 + negy, 0.1);
	}
	/////////////***************************************

	lua.runEventWithParams(posRenderPlayerSEvent, m);
}

void CGamePlay::update()
{
	std::lock_guard<std::mutex> l(GPPGame::playersMutex);

    double now = CEngine::engine().getTime();
    double delta = now - updateLastTimeCalled;
    updateLastTimeCalled = now;
    iFPSCount++;

    if (now - lastiFPSUpdated > 1.0)
    {
        iFPS = iFPSCount;
        iFPSCount = 0;
        lastiFPSUpdated = now;
    }
	////////////////////////////////////////

	for (auto &pp : players)
	{
		auto &p = *pp;
		if (p.bUpdateP)
        {
            if (!bIsACharterGP) p.musicRunningTime += delta * gSpeed;
            updatePlayer(p, delta);
        }
	}
}

void CGamePlay::marathonUpdate()
{
    double now = CEngine::engine().getTime();
    double delta = now - updateLastTimeCalled;
    updateLastTimeCalled = now;
    iFPSCount++;

    if (now - lastiFPSUpdated > 1.0)
    {
        iFPS = iFPSCount;
        iFPSCount = 0;
        lastiFPSUpdated = now;
    }
	////////////////////////////////////////

	//std::lock_guard<std::mutex> l(GPPGame::playersMutex);
	for (auto &pp : players)
	{
		auto &p = *pp;
        if (!bIsACharterGP) p.musicRunningTime += delta * gSpeed;
		if (p.bUpdateP && !p.isSongChartFinished()) updatePlayer(p, delta);
	}
}

void CGamePlay::resetModule()
{
	fretboardLightFade = 20.0;
	bFretboardLightFading = true;

	for (auto &p : players)
	{
		p->resetSongThings();
	}

	players.clear();
	chartInstruments.clear();

	songlyrics.clear();

	songlyricsIndex = 0;
}

void CGamePlay::render()
{
	auto &game = GPPGame::GuitarPP();
	auto &engine = CEngine::engine();
	
	for (auto &p : players)
	{
		if (p->bRenderP) renderPlayer(*p);
	}
    
	if (game.showTextsTest)
    {
		CFonts::fonts().drawTextInScreenWithBuffer(std::to_string(engine.getFPS()) + " FPS", 0.8, 0.8, 0.1);

        static int lastInternalFPS = 0;
        static double lastInternalFPST = engine.getTime();

        double gtime = engine.getTime();

        if ((gtime - lastInternalFPST) > 1.0)
        {
            lastInternalFPST = gtime;
            lastInternalFPS = (int)(1.0 / (gtime - updateLastTimeCalled));
        }
		
        CFonts::fonts().drawTextInScreenWithBuffer(std::to_string(iFPS) + "i FPS", 0.8, 0.6, 0.1);
    }
}

bool CGamePlay::renderBackground()
{
	{
			/////////////////
            /*const char *frmbgtest = vid.getFrame(updatedTestBG);
            if (updatedTestBG)
            {
                engine.uploadBytesToGl(bgVideoText, frmbgtest, vid.width, vid.height);
            }
            
            engine.Render2DQuad(RenderData);*/

		engine.activate3DRender(true);
		engine.activateLighting(true);

		{
			double centerx = 0.0;
			double centerz = -650.0;

			double rtime = getBPlayer().musicRunningTime / 10.0;
			double eyexcam = sin(0) * 1.0 + centerx + sin(rtime) * 1.0;
			double eyezcam = cos(0) * 1.0 + centerz + cos(rtime) * 1.0;

			CEngine::cameraSET usingCamera;
            
			usingCamera.eye.x = 3.0 + sin(rtime);
			usingCamera.eye.y = 2.5;
			usingCamera.eye.z = 1.0 + cos(rtime);
			usingCamera.center.x = 3.0;
			usingCamera.center.y = 0.5;
			usingCamera.center.z = -5;
			usingCamera.up.x = 0;
			usingCamera.up.y = 1;
			usingCamera.up.z = 0;

			engine.setCamera(usingCamera);
		}

		{
			lightData l;

			for (auto &t : l.ambientLight)
			{
				t = 0.1f;
			}

			for (auto &t : l.direction)
			{
				t = 2.5f;
			}

			for (auto &t : l.position)
			{
			    t = 0.0f;
			}

			for (auto &t : l.specularLight)
			{
				t = 0.2f;
			}
            
			for (auto &t : l.diffuseLight)
			{
				t = 0.2f;
			}

			l.specularLight[1] = 1.0f;
			l.specularLight[2] = 1.0f;
			l.diffuseLight[0] = 1.0f;
			l.diffuseLight[1] = 1.0f;
			l.ambientLight[3] = 0.1f;

			CEngine::colorRGBToArrayf(0xFFF6ED, l.diffuseLight);

			l.angle = 180.0f;
			l.direction[0] = 3.0f;
			l.direction[1] = -0.5f;
			l.direction[2] = -1.5f;

			l.position[0] = 3.0f;
			l.position[1] = 2.7f;
			l.position[2] = -1.5f;
			l.position[3] = 1.0f;

			engine.activateLight(0, false);
			engine.activateLight(1, true);
			engine.setLight(l, 1);
		}

		engine.activateNormals(true);
		GPPGame::GuitarPP().testobj.draw(0);
		engine.activateNormals(false);

		engine.activateLighting(false);
		engine.activate3DRender(false);

		engine.matrixReset();

		engine.clear3DBuffer();
	}
    return true;
}

void CGamePlay::startUpdateDelta()
{
    updateLastTimeCalled = CEngine::engine().getTime();
}

CGamePlay::CGamePlay() : engine(CEngine::engine())
{
	auto &Lua = CLuaH::Lua();
    iFPS = 0;
    iFPSCount = 0;
	preRenderPlayerSEvent = Lua.idForCallbackEvent("preRenderPlayer");
	posRenderPlayerSEvent = Lua.idForCallbackEvent("posRenderPlayer");
    updateLastTimeCalled = 0.0;

	fretboardLightFade = 20.0;
	bFretboardLightFading = true;
	bRenderHUD = true;
	bIsACharterGP = false;
	showBPMVlaues = false;
	speedMp = 2.5; // equivalent to Guitar Hero's hyperspeed

	gSpeed = 1.0; // music speed

	hopoLightText = GPPGame::GuitarPP().loadTexture("data/sprites", "hopolight.tga").getTextId();

	songlyricsIndex = 0;

	//fretsTextures = "default";
	//fretsText = GPPGame::GuitarPP().frets[fretsTextures];
	//fretsText.notesTexture = GPPGame::GuitarPP().loadTexture("data/sprites", "Strums.tga").getTextId();
	//fretsText.strikeLineTexture = GPPGame::GuitarPP().loadTexture("data/sprites", "frets.tga").getTextId();
	fireText = GPPGame::GuitarPP().loadTexture("data/sprites", "flamea.tga").getTextId();
	pfireText = GPPGame::GuitarPP().loadTexture("data/sprites", "fire.tga").getTextId();

	BPMLineText = "v.tga";

	BPMTextID = GPPGame::GuitarPP().loadTexture("data/sprites", BPMLineText).getTextId();

	showBPMLines = true;

	for (auto &al : hoposLight.ambientLight)
	{
		al = 0.0;
	}

	for (auto &al : hoposLight.diffuseLight)
	{
		al = 1.0;
	}

	hoposLight.diffuseLight[3] = 0.2f;

	for (auto &al : hoposLight.specularLight)
	{
		al = 1.0;
	}

	hoposLight.specularLight[3] = 0.2f;

	plusNoteLight = hoposLight;

	plusNoteLight.diffuseLight[0] = 0.2f;
	plusNoteLight.diffuseLight[1] = 0.2f;
	plusNoteLight.specularLight[0] = 0.2f;
	plusNoteLight.specularLight[1] = 0.2f;

	hoposLight.angle = 90.0f;

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



