/*****************************************************************************************************
*		GUITAR++
*		Old CFonts class
*		PROGRAMADO POR FÁBIO
*		BMS - Brazilian Modding Studio - http://brmodstudio.forumeiros.com
*****************************************************************************************************/
#pragma once
#include "CEngine.h"
#include <vector>
#include <deque>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

struct TextPositionStruct{
	int FontID;
	double x, y, size;
	std::string Text;
	
	TextPositionStruct(){
		FontID = 0;
	}
};

class CFonts{
public:
	int MaxCharsInTexture;

	struct TimedText{
		char *Text;
		double time, AddedTime;
		double posX1, posY1, size;
		bool Enabled;
	};
	
	struct chartable{
		int pos;
		int line;
		
		chartable(){
			pos = -1;
			line = 0;
		}
	};
	
	chartable TableOfChars[256];
	
	double SizeOfChar, sizeOfLine;
	int lines;
	
	std::deque<TimedText> Texts;

	~CFonts();
	
	unsigned int Texture;
	
	inline int GetIndexOfChar(int character){
		if(character < 0) character = 128 + (128 - character * -1);
		
		character &= 0xFF;
		
		return TableOfChars[character].pos;
	}
	
	inline int getCharLine(int character){
		if(character < 0) character = 128 + (128 - character * -1);
		
		character &= 0xFF;
		
		
		return TableOfChars[character].line;
	}
	
	double getCenterPos(const std::string &str, double size, double posX1);
	double getCenterPos(const char *str, double size, double posX1);
	double getCenterPos(int charsnum, double size, double posX1);
	int GetCharSize(char* _char);
	int ShowTimedText(const char *Text, double time, double posX1, double posY1, double size);
	int ShowTimedText(std::string &Text, double time, double posX1, double posY1, double size);
	void DrawStoredText();
	void DisableStoredText(int ID);
	void EnableStoredText(int ID);
	void drawText(const char *str, const double posX1, const double posY1, const double size);
	void DrawTextInGLFWWindow(std::string mstr, const double posX1, const double posY1, const double size);
	
	void operator << (TextPositionStruct *info){DrawTextInGLFWWindow((char*)info->Text.c_str(), info->x, info->y, info->size);}
	void operator << (const TextPositionStruct &info){DrawTextInGLFWWindow((char*)info.Text.c_str(), info.x, info.y, info.size);}
	bool TextIsActive(int ID);

	static CFonts &fonts();

private:
	CFonts(const CFonts&) = delete;
	CFonts();
};
