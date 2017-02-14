/*****************************************************************************************************
*		GUITAR++
*		PROGRAMADO POR FÁBIO
*		BMS - Brazilian Modding Studio - http://brmodstudio.forumeiros.com
*****************************************************************************************************/
#pragma once
#ifndef __GUITARPP_CFONTS_H_
#define __GUITARPP_CFONTS_H_

#include "CEngine.h"
#include <vector>
#include <deque>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

class CFonts{
	std::map < unsigned int, CEngine::dTriangleWithAlpha > textPerTextureBuffer;

public:
	class Font{
		class fontTexture{
			friend Font;

			std::string name;
			int lines, columns;

		public:
			std::string getName() const
			{
				return name;
			}

			inline int getlines() const
			{
				return lines;
			}

			inline int getcolumns() const
			{
				return columns;
			}


			void load(const std::string &path, const std::string &name);

			fontTexture(const std::string &path, const std::string &name);
			fontTexture();
		};


		class chartbl{
			int pos;
			int line;
			double align, size;
			fontTexture *textureLst;
			unsigned int textID;

			void setTextID(const fontTexture &texture);

		public:
			void internalProcessTexture(int ch);

			inline double getAlign()
			{
				return align;
			}

			inline double getSize()
			{
				return size;
			}

			inline void setLine(int l){
				line = l;
			}

			inline int getline() const{
				return line;
			}

			inline void setPos(int p){
				pos = p;
			}

			inline int getPos() const{
				return pos;
			}

			inline void setText(const fontTexture &texture){
				textureLst = const_cast<fontTexture*>(&texture);
				setTextID(texture);
			}

			inline fontTexture *getText() const{
				return textureLst;
			}

			inline unsigned int getTextID() const{
				return textID;
			}

			chartbl(){
				pos = -1;
				line = 0;
				textureLst = nullptr;
				align = 0;
				size = 1.0;
			}
		};

		std::map<std::string, fontTexture>					textures;
		std::map<unsigned int, chartbl>						chars;
		friend CFonts;

	public:
		
		void registerTexture(const std::string &path, const std::string &texture, const std::wstring &textChars);
		void registerTexture(const std::string &path, const std::string &texture, const std::string &textChars);
		Font();
	};

	static size_t utf8Size(const std::string &s);
	static size_t utf8InsertAt(std::string &s, const std::string &str, size_t at);

	// Adds Texture to font and create a font inst if doesnt exists
	std::string							addTextureToFont(const std::string &fontName, const std::string &path, const std::string &texture, const std::wstring &textChars);
	std::string							addTextureToFont(const std::string &fontName, const std::string &path, const std::string &texture, const std::string &textChars);
	double								getCenterPos(const std::string &text, double size, double posX1, const std::string &fontName = "default");
	
	void								drawTextInScreenWithBuffer(const std::string &str, const double posX1, const double posY1, const double size, const std::string &fontName = "default");
	void								drawTextInScreen(const std::string &str, const double posX1, const double posY1, const double size, const std::string &fontName = "default");
	void								draw3DTextInScreen(const std::string &str, const double posX1, const double posY1, const double posZ1, const double sizeX, const double sizeY, const double sizeZ, const std::string &fontName = "default");

	void								drawAllBuffers();

	static CFonts &fonts();

private:
	std::map <std::string, Font> fontsReg;

	CFonts(const CFonts&) = delete;
	CFonts();
};

#endif
