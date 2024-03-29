/*****************************************************************************************************
 *		GUITAR++
 *		PROGRAMADO POR FÁBIO
 *		BMS - Brazilian Modding Studio -
 *http://brmodstudio.forumeiros.com
 *****************************************************************************************************/
#pragma once
#ifndef __GUITARPP_CFONTS_H_
#define __GUITARPP_CFONTS_H_

#include "CEngine.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <vector>

class CFonts {
    std::map<unsigned int, CEngine::dTriangleWithAlpha> textPerTextureBuffer;

  public:
    struct textAlert {
        int status;
        double startTime;
        std::string msg;
        std::function<bool(textAlert &)> callback;

        textAlert() noexcept {
            status = 0;
            startTime = 0.0;
        }

        ~textAlert() noexcept {}
    };

    class Font {
        class fontTexture {
            friend Font;

            std::string name;
            int lines, columns;

          public:
            const std::string &getName() const noexcept { return name; }

            inline int getlines() const noexcept { return lines; }

            inline int getcolumns() const noexcept { return columns; }

            void load(const std::string &path, const std::string &name);

            fontTexture(const std::string &path, const std::string &name);
            fontTexture() noexcept;
            ~fontTexture() noexcept {}
        };

        class chartbl {
            int pos;
            int line;
            double align, size;
            const fontTexture *textureLst;
            unsigned int textID;

            void setTextID(const fontTexture &texture);

          public:
            void internalProcessTexture(int ch);

            inline double getAlign() const noexcept { return align; }

            inline double getSize() const noexcept { return size; }

            inline void setLine(int l) noexcept { line = l; }

            inline int getline() const noexcept { return line; }

            inline void setPos(int p) noexcept { pos = p; }

            inline int getPos() const noexcept { return pos; }

            inline void setText(const fontTexture &texture) {
                textureLst = &texture;
                setTextID(texture);
            }

            inline const fontTexture *getText() const noexcept {
                return textureLst;
            }

            inline unsigned int getTextID() const noexcept { return textID; }

            chartbl() noexcept {
                pos = -1;
                line = 0;
                textureLst = nullptr;
                align = 0;
                size = 1.0;
            }
        };

        std::map<std::string, fontTexture> textures;
        std::map<unsigned int, chartbl> chars;
        friend CFonts;

      public:
        void registerTexture(const std::string &path,
                             const std::string &texture,
                             const std::wstring &textChars);
        void registerTexture(const std::string &path,
                             const std::string &texture,
                             const std::string &textChars);
        Font();
    };

    static size_t utf8Size(const std::string &s);
    static size_t utf8InsertAt(std::string &s, const std::string &str,
                               size_t at);
    static void utf8RemoveLast(std::string &s);
    static void utf8RemoveAtRange(std::string &s, int at, int size);

    // Adds Texture to font and create a font inst if doesnt exists
    std::string addTextureToFont(const std::string &fontName,
                                 const std::string &path,
                                 const std::string &texture,
                                 const std::wstring &textChars);
    std::string addTextureToFont(const std::string &fontName,
                                 const std::string &path,
                                 const std::string &texture,
                                 const std::string &textChars);
    double getCenterPos(const std::string &text, double size, double posX1,
                        const std::string &fontName = "default");
    double getXSizeInScreen(const std::string &text, double size,
                            const std::string &fontName = "default");

    void drawTextInScreenWithBuffer(const std::string &str, const double posX1,
                                    const double posY1, const double size,
                                    const std::string &fontName = "default");
    void drawTextInScreen(const std::string &str, const double posX1,
                          const double posY1, const double size,
                          const std::string &fontName = "default");
    void draw3DTextInScreen(const std::string &str, const double posX1,
                            const double posY1, const double posZ1,
                            const double sizeX, const double sizeY,
                            const double sizeZ,
                            const std::string &fontName = "default");

    void drawAllBuffers();

    void addTextAlert(const textAlert &t);
    void addTextAlert(textAlert &&t);

    static CFonts &fonts();

  private:
    std::map<std::string, Font> fontsReg;
    std::deque<textAlert> textAlerts;

    CFonts(const CFonts &) = delete;
    CFonts();
};

#endif
