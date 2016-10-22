/*****************************************************************************************************
*		GUITAR++
*		PROGRAMADO POR FÁBIO
*		BMS - Brazilian Modding Studio - http://brmodstudio.forumeiros.com
*****************************************************************************************************/
#include "CFonts.h"
#include <iostream>
#include "GPPGame.h"
#include "CEngine.h"
#include "utf8.h"
#include "CLog.h"

CFonts &CFonts::fonts(){
	static CFonts fn;
	return fn;
}

void CFonts::Font::fontTexture::load(const std::string &path, const std::string &name)
{
	this->name = GPPGame::GuitarPP().loadTexture(path, name).getGTextureName();
	lines = columns = 0;
}

CFonts::Font::fontTexture::fontTexture(const std::string &path, const std::string &name)
{
	this->name = GPPGame::GuitarPP().loadTexture(path, name).getGTextureName();
	lines = columns = 0;
}

CFonts::Font::fontTexture::fontTexture()
{
	lines = columns = 0;
}

double CFonts::getCenterPos(int charsnum, double size, double posX1)
{
	return posX1 - (((double)(charsnum)* size / 1.5) + (size / 2.0)) / 2.0;
}

void CFonts::Font::registerTexture(const std::string &path, const std::string &texture, const std::wstring &textChars)
{
	const std::string name = (path + "/" + texture);
	textures[name].load(path, texture);

	auto &tex = textures[name];

	int pos = 0;

	tex.lines = 1;

	auto it = textChars.begin();

	for (auto ch = utf8::next(it, textChars.end()); it != textChars.end(); ch = utf8::next(it, textChars.end()))
	{
		switch (ch)
		{
		case '\n':
			tex.lines++;

			if (tex.getcolumns() < pos){
				tex.columns = pos;
			}

			pos = 0;
			break;

		default:
			auto &charInst = chars[ch];

			charInst.setLine(tex.lines - 1);
			charInst.setPos(pos++);
			charInst.setText(tex);

			break;
		}
	}
}

void CFonts::Font::registerTexture(const std::string &path, const std::string &texture, const std::string &textChars)
{
	const std::string name = (path + "/" + texture);
	textures[name].load(path, texture);

	auto &tex = textures[name];

	int pos = 0;

	tex.lines = 1;

	auto it = textChars.begin();

	for (auto ch = utf8::next(it, textChars.end()); it != textChars.end(); ch = utf8::next(it, textChars.end()))
	{
		switch (ch)
		{
		case '\n':
			tex.lines++;

			if (tex.getcolumns() < pos){
				tex.columns = pos;
			}

			pos = 0;
			break;

		default:
			auto &charInst = chars[ch];

			charInst.setLine(tex.lines - 1);
			charInst.setPos(pos++);
			charInst.setText(tex);

			break;
		}
	}
}

void CFonts::Font::chartbl::setTextID(const fontTexture &texture)
{
	textID = GPPGame::GuitarPP().gTextures[texture.getName()].getTextId();
}

void CFonts::draw3DTextInScreen(const std::string &str, const double posX1, const double posY1, const double posZ1, const double sizeX, const double sizeY, const double sizeZ, const std::string &fontName)
{
	auto &fontToUse = fontsReg[fontName];

	double CharPos = 0.0;
	const double sizeDiv1_5 = sizeX / 1.5, sizeDiv2_0 = sizeX / 2.0;
	const double posX1PlusSizeDiv2_0 = posX1 + sizeDiv2_0;

	CEngine::RenderDoubleStruct RenderData;

	auto &engine = CEngine::engine();

	RenderData.y1 = posY1 + sizeY;
	RenderData.y2 = posY1 + sizeY;
	RenderData.y3 = posY1;
	RenderData.y4 = posY1;

	RenderData.z1 = posZ1 + sizeZ;
	RenderData.z2 = posZ1 + sizeZ;
	RenderData.z3 = posZ1;
	RenderData.z4 = posZ1;

	int i = 0;

	const std::string st = str + " ";

	auto it = st.begin();

	for (auto ch = utf8::next(it, st.end()); it != st.end(); ch = utf8::next(it, st.end()))
	{
		auto &chData = fontToUse.chars[ch];

		if (chData.getText()){
			auto text = chData.getTextID();
			auto &fontsTextData = *chData.getText();

			const double positionFromCharInTexture = chData.getPos();
			const double sizeFromChar = 1.0 / (double)fontsTextData.getcolumns();
			const double sizeOfLine = 1.0 / (double)fontsTextData.getlines();

			RenderData.Text = text;

			CharPos = posX1PlusSizeDiv2_0 + (((double)i) * sizeDiv1_5);

			RenderData.x1 = CharPos;
			RenderData.x2 = CharPos + sizeX;
			RenderData.x3 = CharPos + sizeX;
			RenderData.x4 = CharPos;

			RenderData.TextureY1 = 1.0 - (sizeOfLine * chData.getline());
			RenderData.TextureY2 = RenderData.TextureY1 - sizeOfLine;

			RenderData.TextureX1 = (positionFromCharInTexture * sizeFromChar);
			RenderData.TextureX2 = (positionFromCharInTexture * sizeFromChar) + sizeFromChar;

			engine.Render3DQuad(RenderData);
		}

		++i;
	}
}

void CFonts::drawAllBuffers()
{
	auto &engine = CEngine::engine();

	for (auto &b : textPerTextureBuffer)
	{
		b.second.texture = b.first;
		engine.drawTrianglesWithAlpha(b.second);
		b.second.clear();
	}
}

size_t CFonts::utf8Size(const std::string &s)
{
	std::string st = s + " ";
	auto it = st.begin();
	size_t size = 0;

	for (auto ch = utf8::next(it, st.end()); it != st.end(); ch = utf8::next(it, st.end()))
	{
		size++;
	}

	return size;
}

size_t CFonts::utf8InsertAt(std::string &s, const std::string &str, size_t at)
{
	if (at == (~0))
	{
		at = 0;
	}

	size_t size = utf8Size(s);

	if (size < at)
	{
		at = size;
	}

	auto it = s.begin();

	try{

		if (at > 0)
		{
			utf8::advance(it, at, s.end());
		}

		s.insert(it, str.begin(), str.end());
	}
	catch (const std::exception &e)
	{
		CLog::log() << e.what();
	}

	size_t sz = utf8Size(str);

	for (auto sIt = s.begin(); sIt != it; ++sIt)
	{
		++sz;
	}

	size = utf8Size(s);

	if (size < sz)
	{
		sz = size;
	}

	return sz;
}

void CFonts::drawTextInScreenWithBuffer(const std::string &str, const double posX1, const double posY1, const double size, const std::string &fontName)
{
	auto &fontToUse = fontsReg[fontName];

	double CharPos = 0.0;
	const double sizeDiv1_5 = size / 1.5, sizeDiv2_0 = size / 2.0;
	const double posX1PlusSizeDiv2_0 = posX1 + sizeDiv2_0;

	auto &engine = CEngine::engine();
	CEngine::RenderDoubleStruct RenderData;

	RenderData.y1 = posY1 + size;
	RenderData.y2 = posY1 + size;
	RenderData.y3 = posY1;
	RenderData.y4 = posY1;


	RenderData.z1 = 0.0;
	RenderData.z2 = 0.0;
	RenderData.z3 = 0.0;
	RenderData.z4 = 0.0;

	int i = 0;

	const std::string st = str + " ";
	static CEngine::dTriangleWithAlpha *buffer = nullptr;
	static unsigned int lasttext = 0;

	auto it = st.begin();

	for (auto ch = utf8::next(it, st.end()); it != st.end(); ch = utf8::next(it, st.end()))
	{
		auto &chData = fontToUse.chars[ch];

		if (chData.getText())
		{
			RenderData.Text = chData.getTextID();
			if (RenderData.Text != lasttext)
			{
				buffer = &textPerTextureBuffer[chData.getTextID()];
				lasttext = RenderData.Text;
				buffer->useColors = false;
			}

			if (buffer == nullptr)
			{
				continue;
			}

			auto &fontsTextData = *chData.getText();

			const double positionFromCharInTexture = chData.getPos();
			const double sizeFromChar = 1.0 / (double)fontsTextData.getcolumns();
			const double sizeOfLine = 1.0 / (double)fontsTextData.getlines();

			CharPos = posX1PlusSizeDiv2_0 + (((double)i) * sizeDiv1_5);

			RenderData.x1 = CharPos;
			RenderData.x2 = CharPos + size;
			RenderData.x3 = CharPos + size;
			RenderData.x4 = CharPos;

			RenderData.TextureY1 = 1.0 - (sizeOfLine * chData.getline());
			RenderData.TextureY2 = RenderData.TextureY1 - sizeOfLine;

			RenderData.TextureX1 = (positionFromCharInTexture * sizeFromChar);
			RenderData.TextureX2 = (positionFromCharInTexture * sizeFromChar) + sizeFromChar;

			//engine.Render2DQuad(RenderData);

			CEngine::pushQuad(*buffer, RenderData);
		}

		++i;
	}
}

void CFonts::drawTextInScreen(const std::string &str, const double posX1, const double posY1, const double size, const std::string &fontName)
{
	auto &fontToUse = fontsReg[fontName];

	double CharPos = 0.0;
	const double sizeDiv1_5 = size / 1.5, sizeDiv2_0 = size / 2.0;
	const double posX1PlusSizeDiv2_0 = posX1 + sizeDiv2_0;

	auto &engine =  CEngine::engine();
	CEngine::RenderDoubleStruct RenderData;

	RenderData.y1 = posY1 + size;
	RenderData.y2 = posY1 + size;
	RenderData.y3 = posY1;
	RenderData.y4 = posY1;

	int i = 0;

	const std::string st = str + " ";

	auto it = st.begin();

	for (auto ch = utf8::next(it, st.end()); it != st.end(); ch = utf8::next(it, st.end()))
	{
		auto &chData = fontToUse.chars[ch];

		if (chData.getText())
		{
			RenderData.Text = chData.getTextID();
			auto &fontsTextData = *chData.getText();

			const double positionFromCharInTexture = chData.getPos();
			const double sizeFromChar = 1.0 / (double)fontsTextData.getcolumns();
			const double sizeOfLine = 1.0 / (double)fontsTextData.getlines();

			CharPos = posX1PlusSizeDiv2_0 + (((double)i) * sizeDiv1_5);

			RenderData.x1 = CharPos;
			RenderData.x2 = CharPos + size;
			RenderData.x3 = CharPos + size;
			RenderData.x4 = CharPos;

			RenderData.TextureY1 = 1.0 - (sizeOfLine * chData.getline());
			RenderData.TextureY2 = RenderData.TextureY1 - sizeOfLine;

			RenderData.TextureX1 = (positionFromCharInTexture * sizeFromChar);
			RenderData.TextureX2 = (positionFromCharInTexture * sizeFromChar) + sizeFromChar;

			engine.Render2DQuad(RenderData);
		}

		++i;
	}
}

std::string CFonts::addTextureToFont(const std::string &fontName, const std::string &path, const std::string &texture, const std::wstring &textChars)
{
	auto &font = fontsReg[fontName];

	font.registerTexture(path, texture, textChars);

	return (path + "/" + texture);
}

std::string CFonts::addTextureToFont(const std::string &fontName, const std::string &path, const std::string &texture, const std::string &textChars)
{
	auto &font = fontsReg[fontName];

	font.registerTexture(path, texture, textChars);

	return (path + "/" + texture);
}

CFonts::Font::Font()
{


}

CFonts::CFonts()
{
	addTextureToFont("default", "data/sprites", "FONT.tga", "0123456789ABCDEFGHIJKLMNOPQRSTUV\nWXYZabcdefghijklmnopqrstuvwxyz.,\nÃÂÀÁÄÊÈÉËÎÌÍÏÕÔ\n?----\n---");
	// ÃÂÀÁÄÊÈÉËÎÌÍÏÕÔÓÒÖ\xFFÛ

}

