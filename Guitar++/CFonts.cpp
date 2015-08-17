/*****************************************************************************************************
*		GUITAR++
*		PROGRAMADO POR FÁBIO
*		BMS - Brazilian Modding Studio - http://brmodstudio.forumeiros.com
*****************************************************************************************************/
#include "CFonts.h"
#include <iostream>
#include "GPPGame.h"
#include "CEngine.h"

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

void CFonts::Font::registerTexture(const std::string &path, const std::string &texture, const std::string &textChars)
{
	const std::string name = (path + "/" + texture);
	textures[name].load(path, texture);

	auto &tex = textures[name];

	int pos = 0;

	tex.lines = 1;

	for (auto &ch : textChars){
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
			const std::string charCode = std::to_string((int)ch);
			auto &charInst = chars[charCode];

			charInst.setLine(tex.lines - 1);
			charInst.setPos(pos++);
			charInst.setText(tex);

			break;
		}
	}


}

void CFonts::drawTextInScreen(const std::string &str, const double posX1, const double posY1, const double size, const std::string &fontName)
{
	auto &fontToUse = fontsReg[fontName];

	double CharPos = 0.0;
	const double sizeDiv1_5 = size / 1.5, sizeDiv2_0 = size / 2.0;
	const double posX1PlusSizeDiv2_0 = posX1 + sizeDiv2_0;

	CEngine::RenderDoubleStruct RenderData;

	RenderData.y1 = posY1;
	RenderData.y2 = posY1;
	RenderData.y3 = posY1 + size;
	RenderData.y4 = posY1 + size;

	int i = 0;

	for (auto &ch : str){
		auto &chData = fontToUse.chars[std::to_string((int)ch)];

		if (chData.getText()){
			auto &text = GPPGame::GuitarPP().gTextures[chData.getText()->getName()];
			auto &fontsTextData = *chData.getText();

			const double positionFromCharInTexture = chData.getPos();
			const double sizeFromChar = 1.0 / (double)fontsTextData.getcolumns();
			const double sizeOfLine = 1.0 / (double)fontsTextData.getlines();

			RenderData.Text = text.getTextId();

			CharPos = posX1PlusSizeDiv2_0 + (((double)i) * sizeDiv1_5);

			RenderData.x1 = CharPos;
			RenderData.x2 = CharPos + size;
			RenderData.x3 = CharPos + size;
			RenderData.x4 = CharPos;

			RenderData.TextureY1 = sizeOfLine - (sizeOfLine * chData.getline());
			RenderData.TextureY2 = RenderData.TextureY1 + sizeOfLine;

			RenderData.TextureX1 = (positionFromCharInTexture * sizeFromChar);
			RenderData.TextureX2 = (positionFromCharInTexture * sizeFromChar) + sizeFromChar;

			CEngine::engine().Render2DQuad(RenderData);
		}

		++i;
	}
}

std::string CFonts::addFont(const std::string &fontName, const std::string &path, const std::string &texture, const std::string &textChars)
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
	addFont("default", "data/sprites", "FONT.tga", "0123456789ABCDEFGHIJKLMNOPQRSTUV\nWXYZabcdefghijklmnopqrstuvwxyz");


}

