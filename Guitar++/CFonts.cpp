/*****************************************************************************************************
*		GUITAR++
*		PROGRAMADO POR FÁBIO
*		BMS - Brazilian Modding Studio - http://brmodstudio.forumeiros.com
*****************************************************************************************************/
#include "CFonts.h"
#include <iostream>

CFonts &CFonts::fonts(){
	static CFonts fn;
	return fn;
}

CFonts::CFonts(){
	MaxCharsInTexture = 32;
	SizeOfChar = 1.0 / ((double)MaxCharsInTexture);
	lines = 2;
	sizeOfLine = 1.0 / ((double)lines);

	static char tmp[]={48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 0};
	
	TableOfChars[(int)'0'].pos = 0;
	TableOfChars[(int)'1'].pos = 1;
	TableOfChars[(int)'2'].pos = 2;
	TableOfChars[(int)'3'].pos = 3;
	TableOfChars[(int)'4'].pos = 4;
	TableOfChars[(int)'5'].pos = 5;
	TableOfChars[(int)'6'].pos = 6;
	TableOfChars[(int)'7'].pos = 7;
	TableOfChars[(int)'8'].pos = 8;
	TableOfChars[(int)'9'].pos = 9;
	//TableOfChars[(int)'A'].pos = 10;
	TableOfChars[(int)'A'].pos = 10;
	TableOfChars[(int)'B'].pos = 11;
	TableOfChars[(int)'C'].pos = 12;
	TableOfChars[(int)'D'].pos = 13;
	TableOfChars[(int)'E'].pos = 14;
	TableOfChars[(int)'F'].pos = 15;
	TableOfChars[(int)'G'].pos = 16;
	TableOfChars[(int)'H'].pos = 17;
	TableOfChars[(int)'I'].pos = 18;
	TableOfChars[(int)'J'].pos = 19;
	TableOfChars[(int)'K'].pos = 20;
	TableOfChars[(int)'L'].pos = 21;
	TableOfChars[(int)'M'].pos = 22;
	TableOfChars[(int)'N'].pos = 23;
	TableOfChars[(int)'O'].pos = 24;
	TableOfChars[(int)'P'].pos = 25;
	TableOfChars[(int)'Q'].pos = 26;
	TableOfChars[(int)'R'].pos = 27;
	TableOfChars[(int)'S'].pos = 28;
	TableOfChars[(int)'T'].pos = 29;
	TableOfChars[(int)'U'].pos = 30;
	TableOfChars[(int)'V'].pos = 31;
	TableOfChars[(int)'W'].pos = 0;
	TableOfChars[(int)'W'].line = 1;
	TableOfChars[(int)'X'].pos = 1;
	TableOfChars[(int)'X'].line = 1;
	TableOfChars[(int)'Y'].pos = 2;
	TableOfChars[(int)'Y'].line = 1;
	TableOfChars[(int)'Z'].pos = 3;
	TableOfChars[(int)'Z'].line = 1;/*
	TableOfChars[(int)'a'].pos = 4;
	TableOfChars[(int)'a'].line = 1;
	TableOfChars[(int)'x'].pos = 4 + ('x' - 'a');
	TableOfChars[(int)'x'].line = 1;*/
	
	for(int i = 'a'; i <= 'z'; i++){
		TableOfChars[i].pos = 4 + i - 'a';
		TableOfChars[i].line = 1;
	}
	
	fontMap = nullptr;
	
	//fontMap = new char[nChars + 1];
	//strcpy(fontMap, tmp, sizeof(tmp));
	
	Texture = CEngine::inst().loadTexture("data/sprites/FONT.tga");
}

double CFonts::getCenterPos(int charsnum, double size, double posX1){
	/*
	while(charn < textsize){
		CharPos = posX1+(((double)charn)*size/1.3)+(size/2.0);
		
		charpos = GetIndexOfChar(str[charn]);
		if(charpos == -1){charn++; continue;}

		if(charpos > 10) charpos--;
		GLdouble Array[]={CharPos, CharPos+size, CharPos+size, CharPos, posY1, posY1, posY1+size, posY1+size, (((double)charpos)*SizeOfChar), 0.0, (((double)charpos)*SizeOfChar)+SizeOfChar, 1.0f};
		CGraphics::thisptr->Render2D(Array[0], Array[1], Array[2], Array[3], Array[4], Array[5], Array[6], Array[7], Array[8], Array[9], Array[10], Array[11], this->Texture);


		charn++;
	}*/
	return posX1 - (((double)(charsnum) * size / 1.5) + (size / 2.0)) / 2.0;
}

double CFonts::getCenterPos(const char *str, double size, double posX1){
	return getCenterPos(strlen(str), size, posX1);
}

double CFonts::getCenterPos(const std::string &str, double size, double posX1){
	return getCenterPos(str.size(), size, posX1);
}

int CFonts::GetCharSize(char* _char){
	char *c = _char;
	while(*_char != 0x0) _char++;

	return (_char+2) - c;
}

int CFonts::ShowTimedText(const char *Text, double time, double posX1, double posY1, double size){
	TimedText nTimedText;
	Texts.push_back(nTimedText);
	Texts.back().AddedTime = CEngine::inst().getTime();
	Texts.back().Text = new char[strlen(Text) + 1];
	strcpy(Texts.back().Text, Text);
	Texts.back().time = time;
	Texts.back().posX1 = posX1;
	Texts.back().posY1 = posY1;
	Texts.back().size = size;
	Texts.back().Enabled = true;

	return Texts.size()-1;
}

int CFonts::ShowTimedText(std::string &Text, double time, double posX1, double posY1, double size){
	TimedText nTimedText;
	Texts.push_back(nTimedText);
	Texts.back().AddedTime = CEngine::inst().getTime();
	Texts.back().Text = new char[Text.size() + 1];
	strcpy(Texts.back().Text, Text.c_str());
	Texts.back().time = time;
	Texts.back().posX1 = posX1;
	Texts.back().posY1 = posY1;
	Texts.back().size = size;
	Texts.back().Enabled = true;

	return Texts.size()-1;
}

void CFonts::DisableStoredText(int ID){
	Texts.at(ID).Enabled = false;
}

void CFonts::EnableStoredText(int ID){
	Texts.at(ID).AddedTime = CEngine::inst().getTime();
	Texts.at(ID).Enabled = true;
}

bool CFonts::TextIsActive(int ID){
	return ((CEngine::inst().getTime() - Texts.at(ID).AddedTime < Texts.at(ID).time || Texts.at(ID).AddedTime == -1) && Texts.at(ID).Enabled);
}

void CFonts::DrawStoredText(){
	int TextsVectorSize = Texts.size();
	for(int i = 0; i < TextsVectorSize; i++){
		TimedText &TextS = Texts.at(i);
		
		if ((CEngine::inst().getTime() - TextS.AddedTime < TextS.time || TextS.AddedTime == -1) && TextS.Enabled){
			DrawTextInGLFWWindow(TextS.Text, TextS.posX1, TextS.posY1, TextS.size);
		}
	}
}

CFonts::~CFonts(){
	//if(fontMap) delete[] fontMap;
}
/*
int CFonts::GetIndexOfChar(int _char){
	for(int i = 0; i < nChars; i++)
		if(fontMap[i] == _char) return i;
	
	if(_char < 0) _char = 128 + (128 - _char * -1);
	if(_char >= 0 && _char <= 255){
		return TableOfChars[_char].pos;
	}
	
	return -1;
}*/

void CFonts::drawText(const char *str, const double posX1, const double posY1, const double size){
	double CharPos, charLineC;
	const double sizeDiv1_3 = size / 1.3, sizeDiv2_0 = size / 2.0;
	const double posX1PlusSizeDiv2_0 = posX1 + sizeDiv2_0;
	
	CEngine::RenderDoubleStruct RenderData;
	
	RenderData.y1 = posY1;
	RenderData.y2 = posY1;
	RenderData.y3 = posY1+size;
	RenderData.y4 = posY1+size;
	RenderData.Text = this->Texture;
	
	const char *p = str;
	
	for(int PositionOfCharInTexture; *str; str++){
		if((PositionOfCharInTexture = GetIndexOfChar(*str)) == -1){continue;}
		
		CharPos = posX1PlusSizeDiv2_0 + (((double)(str - p)) * sizeDiv1_3);
		charLineC = getCharLine(*str);
		
		RenderData.x1 = CharPos;
		RenderData.x2 = CharPos+size;
		RenderData.x3 = CharPos+size;
		RenderData.x4 = CharPos;
		
		RenderData.TextureY1 = sizeOfLine - (sizeOfLine * charLineC);
		RenderData.TextureY2 = RenderData.TextureY1 + sizeOfLine;
		
		RenderData.TextureX1 = (((double)PositionOfCharInTexture) * SizeOfChar);
		RenderData.TextureX2 = (((double)PositionOfCharInTexture) * SizeOfChar) + SizeOfChar;
		
		CEngine::engine().Render2DQuad(RenderData);
	}
}


void CFonts::DrawTextInGLFWWindow(std::string mstr, const double posX1, const double posY1, const double size){
	drawText(mstr.c_str(), posX1, posY1, size);
}
