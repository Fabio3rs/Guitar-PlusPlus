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
#include <unordered_map>
#include <map>

class CFonts{


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
			fontTexture *textureLst;

		public:

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
			}

			inline fontTexture *getText() const{
				return textureLst;
			}

			chartbl(){
				pos = -1;
				line = 0;
				textureLst = nullptr;
			}
		};

		std::map<std::string, fontTexture>					textures;
		std::map<int, chartbl>								chars;
		friend CFonts;

	public:
		
		void registerTexture(const std::string &path, const std::string &texture, const std::wstring &textChars);
		Font();
	};

	// Adds Texture to font and create a font inst if doesnt exists
	std::string							addTextureToFont(const std::string &fontName, const std::string &path, const std::string &texture, const std::wstring &textChars);


	void								drawTextInScreen(const std::string &str, const double posX1, const double posY1, const double size, const std::string &fontName = "default");


	static CFonts &fonts();

private:
	std::map <std::string, Font> fontsReg;

	CFonts(const CFonts&) = delete;
	CFonts();
};

#endif
