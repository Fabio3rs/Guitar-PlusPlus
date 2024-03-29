/*****************************************************************************************************
 *		GUITAR++
 *		PROGRAMADO POR FÁBIO
 *		BMS - Brazilian Modding Studio -
 *http://brmodstudio.forumeiros.com
 *****************************************************************************************************/
#include "CFonts.h"
#include "CEngine.h"
#include "CLog.h"
#include "GPPGame.h"
#include "utf8.h"
#include <cstdint>
#include <iostream>

auto CFonts::fonts() -> CFonts & {
    static CFonts fn;
    return fn;
}

void CFonts::Font::fontTexture::load(const std::string &path,
                                     const std::string &name) {
    bool preValue = GPPGame::GuitarPP().gppTextureKeepBuffer;
    GPPGame::GuitarPP().gppTextureKeepBuffer = true;

    this->name = GPPGame::GuitarPP().loadTexture(path, name).getGTextureName();
    lines = columns = 0;

    GPPGame::GuitarPP().gppTextureKeepBuffer = preValue;
}

CFonts::Font::fontTexture::fontTexture(const std::string &path,
                                       const std::string &name) {
    bool preValue = GPPGame::GuitarPP().gppTextureKeepBuffer;
    GPPGame::GuitarPP().gppTextureKeepBuffer = true;

    this->name = GPPGame::GuitarPP().loadTexture(path, name).getGTextureName();
    lines = columns = 0;

    GPPGame::GuitarPP().gppTextureKeepBuffer = preValue;
}

CFonts::Font::fontTexture::fontTexture() noexcept { lines = columns = 0; }

/*double CFonts::getCenterPos(int charsnum, double size, double posX1)
{
        return posX1 - (((double)(charsnum)* size / 1.5) + (size / 2.0)) / 2.0;
}*/

void CFonts::Font::registerTexture(const std::string &path,
                                   const std::string &texture,
                                   const std::wstring &textChars) {
    const std::string name = (path + "/" + texture);
    textures[name].load(path, texture);

    auto &tex = textures[name];

    int pos = 0;

    tex.lines = 1;

    for (auto it = textChars.begin(); it != textChars.end(); /***/) {
        auto ch = utf8::next(it, textChars.end());

        switch (ch) {
        case '\n':
            tex.lines++;

            if (tex.getcolumns() < pos) {
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

void CFonts::Font::registerTexture(const std::string &path,
                                   const std::string &texture,
                                   const std::string &textChars) {
    const std::string name = (path + "/" + texture);
    textures[name].load(path, texture);

    auto &tex = textures[name];

    int pos = 0;

    tex.lines = 1;

    std::vector<int32_t> charTemp;

    for (auto it = textChars.begin(); it != textChars.end(); /***/) {
        auto ch = utf8::next(it, textChars.end());

        switch (ch) {
        case '\n':
            tex.lines++;

            if (tex.getcolumns() < pos) {
                tex.columns = pos;
            }

            pos = 0;
            break;

        default:
            charTemp.push_back(ch);

            auto &charInst = chars[ch];

            charInst.setLine(tex.lines - 1);
            charInst.setPos(pos++);
            charInst.setText(tex);

            break;
        }
    }

    for (auto &ch : charTemp) {
        auto &charInst = chars[ch];
        charInst.internalProcessTexture(ch);
    }
}

void CFonts::Font::chartbl::setTextID(const fontTexture &texture) {
    textID = GPPGame::GuitarPP().getTextureId(texture.getName());
}

void CFonts::Font::chartbl::internalProcessTexture(int ch) {
    auto &text = GPPGame::GuitarPP().gTextures[textureLst->getName()];

#pragma pack(push, 1)
    struct RGBA {
        unsigned char rgba[4];
    };
#pragma pack(pop)

    static_assert(sizeof(RGBA) == 4, __FILE__ " is not 4 bytes");

    RGBA *imgRGBA = (RGBA *)text.getImageData().Data.get();

    // static std::fstream fs("test.txt", std::ios::binary | std::ios::out |
    // std::ios::trunc);

    static int countP = 0;

    ++countP;

    if (imgRGBA != nullptr) {
        int colsn = textureLst->columns;
        int linesn = textureLst->lines;

        int symbolLine = (linesn - 1) - getline();

        if (linesn == 0 || colsn == 0) {
            {
                return;
            }
        }

        int total = text.getImgHeight() * text.getImgWidth();

        if (!text.getImageData().bRevPixels) {
            // std::reverse(imgRGBA, imgRGBA + total);
            text.getImageData().bRevPixels = true;
        }

        int pixelsPerCol = text.getImgHeight() / linesn;
        int pixelsPerLines = text.getImgWidth();
        int pixelsPerLinesDiv = text.getImgWidth() / colsn;

        int pixelPosCol = pixelsPerCol * (getPos());
        int pixelPosLine = pixelsPerLines * (symbolLine);

        auto getColumn = [&](int col, int line) {};

        auto getLineText = [&](int lineIdNum) {
            int calc = (lineIdNum)*pixelsPerLines;
            calc += symbolLine * pixelsPerCol * pixelsPerLines;
            calc += pixelPosCol;

            return &(imgRGBA[calc]);
        };

        // if (fs.is_open())
        {
            // while (true)
            {
                std::vector<int> colCalcMinMax;

                for (intptr_t i = pixelsPerCol - 1; i >= 0; i--) {
                    auto *lnPtr = getLineText(i);

                    for (int j = 0; j < pixelsPerLinesDiv; j++) {
                        int pixel = /*lnPtr[j].rgba[0] | lnPtr[j].rgba[1] |
                                       lnPtr[j].rgba[2] |*/
                                    lnPtr[j]
                                        .rgba[3];
                        pixel = static_cast<int>(pixel != 0);

                        // fs << pixel << " ";
                        if (pixel != 0) {
                            colCalcMinMax.push_back(j);
                            break;
                        }
                    }

                    // char stra[] = {ch, 0};

                    // fs << stra << std::endl;
                }

                if (colCalcMinMax.empty()) {
                    {
                        return;
                    }
                }

                std::sort(colCalcMinMax.begin(), colCalcMinMax.end());
                double alignD = 0;

                {
                    alignD = (double)colCalcMinMax.front();

                    // std::cout << alignD << "  " <<
                    // (double)colCalcMinMax.back() << std::endl;

                    alignD -= 1.0;

                    if (alignD < 0.0) {
                        {
                            alignD = 0;
                        }
                    }

                    align = alignD / (double)(pixelsPerLinesDiv);
                }

                colCalcMinMax.clear();

                for (intptr_t i = pixelsPerCol - 1; i >= 0; i--) {
                    auto *lnPtr = getLineText(i);

                    for (int j = pixelsPerLinesDiv - 1; j >= 0; j--) {
                        int pixel = /*lnPtr[j].rgba[0] | lnPtr[j].rgba[1] |
                                       lnPtr[j].rgba[2] |*/
                                    lnPtr[j]
                                        .rgba[3];
                        pixel = static_cast<int>(pixel != 0);

                        if (pixel != 0) {
                            colCalcMinMax.push_back(j);
                            break;
                        }
                    }
                }

                std::sort(colCalcMinMax.begin(), colCalcMinMax.end());

                {
                    double d = (double)colCalcMinMax.back();

                    // d += 1.0;

                    if (d > pixelsPerLinesDiv) {
                        d = pixelsPerLinesDiv;
                    }

                    size = (d - alignD) / (double)(pixelsPerLinesDiv);

                    if (size < 0.3) {
                        {
                            size = 0.3;
                        }
                    }
                }

                colCalcMinMax.clear();

                /*if (ch == 'w')
                {
                        std::cout << align << "   " << size << "\n";
                }*/
            }

            // fs.flush();
        }
    }
}

void CFonts::draw3DTextInScreen(const std::string &str, const double posX1,
                                const double posY1, const double posZ1,
                                const double sizeX, const double sizeY,
                                const double sizeZ,
                                const std::string &fontName) {
    auto &fontToUse = fontsReg[fontName];

    double CharPos = 0.0;
    const double sizeDiv1_5 = sizeX / 1.5;
    const double sizeDiv2_0 = sizeX / 2.0;
    const double posX1PlusSizeDiv2_0 = posX1 + sizeDiv2_0;

    CEngine::RenderDoubleStruct RenderData{};

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

    for (auto it = str.begin(); it != str.end(); /***/) {
        auto ch = utf8::next(it, str.end());
        auto &chData = fontToUse.chars[ch];

        if (chData.getText() != nullptr) {
            auto text = chData.getTextID();
            const auto &fontsTextData = *chData.getText();

            const double positionFromCharInTexture = chData.getPos();
            const double sizeFromChar =
                1.0 / (double)fontsTextData.getcolumns();
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
            RenderData.TextureX2 =
                (positionFromCharInTexture * sizeFromChar) + sizeFromChar;

            engine.Render3DQuad(RenderData);
        }

        ++i;
    }
}

void CFonts::drawAllBuffers() {
    auto &engine = CEngine::engine();

    for (auto &b : textPerTextureBuffer) {
        b.second.texture = b.first;

        if (!b.second.tArray.empty() && !b.second.vArray.empty()) {
            engine.drawTrianglesWithAlpha(b.second);
            b.second.clear();
        }
    }

    if (!textAlerts.empty()) {
        bool callbackResult = false;

        {
            auto &t = textAlerts.front();
            callbackResult = t.callback(t);
        }

        if (!callbackResult) {
            textAlerts.pop_front();

            if (!textAlerts.empty()) {
                auto &t = textAlerts.front();
                t.startTime = engine.getTime();
            }
        }
    }
}

void CFonts::addTextAlert(const textAlert &t) { textAlerts.push_back(t); }

void CFonts::addTextAlert(textAlert &&t) { textAlerts.push_back(std::move(t)); }

auto CFonts::utf8Size(const std::string &s) -> size_t {
    size_t size = 0;

    for (auto it = s.begin(); it != s.end(); /***/) {
        auto ch = utf8::next(it, s.end());
        size++;
    }

    return size;
}

auto CFonts::utf8InsertAt(std::string &s, const std::string &str, size_t at)
    -> size_t {
    if (at == (~0)) {
        at = 0;
    }

    size_t size = utf8Size(s);

    if (size < at) {
        at = size;
    }

    auto it = s.begin();

    try {

        if (at > 0) {
            utf8::advance(it, at, s.end());
        }

        s.insert(it, str.begin(), str.end());
    } catch (const std::exception &e) {
        CLog::log() << e.what();
    }

    size_t sz = utf8Size(str);

    for (auto sIt = s.begin(); sIt != it; ++sIt) {
        ++sz;
    }

    size = utf8Size(s);

    if (size < sz) {
        sz = size;
    }

    return sz;
}

void CFonts::utf8RemoveLast(std::string &s) {
    auto it = s.begin();
    auto it0 = s.begin();
    size_t size = 0;

    for (auto ch = utf8::next(it, s.end()); it != s.end();
         ch = utf8::next(it, s.end())) {
        it0 = it;
    }

    s.erase(it0);
}

void CFonts::utf8RemoveAtRange(std::string &s, int at, int size) {
    if (at < 0) {
        {
            return;
        }
    }

    if (size < 0) {
        {
            return;
        }
    }

    if (s.empty()) {
        {
            return;
        }
    }

    auto it = s.begin();
    auto itB = s.begin();
    auto itE = s.end();

    int i = 0;

    int pos = (at + size);

    for (auto ch = utf8::next(it, s.end()); it != s.end();
         ch = utf8::next(it, s.end())) {
        ++i;

        if (i == at) {
            itB = it;
        }

        if (i == pos) {
            itE = it;
            break;
        }
    }

    if (i == pos) {
        itE = it;
    }

    s.erase(itB, itE);
}

void CFonts::drawTextInScreenWithBuffer(const std::string &str,
                                        const double posX1, const double posY1,
                                        const double size,
                                        const std::string &fontName) {
    auto &fontToUse = fontsReg[fontName];

    const double sizeDiv2_0 = size / 2.0;
    const double sizeDiv12_0 = size / 12.0;
    const double sizeDiv10_0 = size / 10.0;
    const double sizeDiv20_0 = size / 20.0;
    const double posX1PlusSizeDiv2_0 = posX1 + sizeDiv2_0;
    double CharPos = posX1;

    auto &engine = CEngine::engine();
    CEngine::RenderDoubleStruct RenderData{};

    RenderData.y1 = posY1 + size;
    RenderData.y2 = posY1 + size;
    RenderData.y3 = posY1;
    RenderData.y4 = posY1;

    RenderData.z1 = 0.0;
    RenderData.z2 = 0.0;
    RenderData.z3 = 0.0;
    RenderData.z4 = 0.0;

    int i = 0;

    static CEngine::dTriangleWithAlpha *buffer = nullptr;
    static unsigned int lasttext = 0;

    for (auto it = str.begin(); it != str.end(); /***/) {
        auto ch = utf8::next(it, str.end());

        auto &chData = fontToUse.chars[ch];

        if (iswblank(ch) != 0) {
            CharPos += size / 1.5;
        } else if (chData.getText() != nullptr) {
            RenderData.Text = chData.getTextID();
            if (RenderData.Text != lasttext) {
                buffer = &textPerTextureBuffer[chData.getTextID()];
                lasttext = RenderData.Text;
                buffer->useColors = false;
            }

            if (buffer == nullptr) {
                continue;
            }

            const auto &fontsTextData = *chData.getText();

            const double positionFromCharInTexture = chData.getPos();
            const double sizeFromChar =
                1.0 / (double)fontsTextData.getcolumns();
            const double sizeOfLine = 1.0 / (double)fontsTextData.getlines();

            if (chData.getAlign() > 0.15) {
                {
                    CharPos += -(chData.getAlign() * size) + sizeDiv10_0;
                }
            }

            if (chData.getAlign() < 0.1) {
                {
                    CharPos += sizeDiv20_0;
                }
            }

            RenderData.x1 = CharPos;
            RenderData.x4 = CharPos;
            RenderData.x2 = CharPos + size;
            RenderData.x3 = CharPos + size;

            double sizCalc = sizeDiv12_0;
            double useSiz = chData.getSize();

            if (useSiz < 0.45) {
                useSiz = 0.45;
            } else if (useSiz > 0.9) {
                sizCalc = 0;
            }

            CharPos += useSiz * size + sizCalc;

            RenderData.TextureY1 = 1.0 - (sizeOfLine * chData.getline());
            RenderData.TextureY2 = RenderData.TextureY1 - sizeOfLine;

            RenderData.TextureX1 = (positionFromCharInTexture * sizeFromChar);
            RenderData.TextureX2 =
                (positionFromCharInTexture * sizeFromChar) + sizeFromChar;

            // engine.Render2DQuad(RenderData);

            CEngine::pushQuad(*buffer, RenderData);
        }

        ++i;
    }
}

void CFonts::drawTextInScreen(const std::string &str, const double posX1,
                              const double posY1, const double size,
                              const std::string &fontName) {
    auto &fontToUse = fontsReg[fontName];

    const double sizeDiv2_0 = size / 2.0;
    const double sizeDiv12_0 = size / 12.0;
    const double sizeDiv10_0 = size / 10.0;
    const double sizeDiv20_0 = size / 20.0;
    const double posX1PlusSizeDiv2_0 = posX1 + sizeDiv2_0;
    double CharPos = posX1;

    auto &engine = CEngine::engine();
    CEngine::RenderDoubleStruct RenderData{};

    RenderData.y1 = posY1 + size;
    RenderData.y2 = posY1 + size;
    RenderData.y3 = posY1;
    RenderData.y4 = posY1;

    int i = 0;

    double lastL = 1.0;

    for (auto it = str.begin(); it != str.end(); /***/) {
        auto ch = utf8::next(it, str.end());
        auto &chData = fontToUse.chars[ch];

        if (iswblank(ch) != 0) {
            CharPos += size / 1.5;
        } else if (chData.getText() != nullptr) {
            RenderData.Text = chData.getTextID();
            const auto &fontsTextData = *chData.getText();

            const double positionFromCharInTexture = chData.getPos();
            const double sizeFromChar =
                1.0 / (double)fontsTextData.getcolumns();
            const double sizeOfLine = 1.0 / (double)fontsTextData.getlines();

            // if (i > 0)
            {
                if (chData.getAlign() > 0.15) {
                    {
                        CharPos += -(chData.getAlign() * size) + sizeDiv10_0;
                    }
                }

                if (chData.getAlign() < 0.1) {
                    {
                        CharPos += sizeDiv20_0;
                    }
                }
            }
            /*if (lastL < 0.8)
                    CharPos += -(chData.getAlign() * size);
            else
                    CharPos += -(chData.getAlign() * size) / 2.0;*/

            // CharPos += size / 10.0;

            RenderData.x1 = CharPos;
            RenderData.x4 = CharPos;
            RenderData.x2 = CharPos + size;
            RenderData.x3 = CharPos + size;

            double sizCalc = sizeDiv12_0;
            double useSiz = chData.getSize();

            if (useSiz < 0.45) {
                useSiz = 0.45;
            } else if (useSiz > 0.9) {
                sizCalc = 0;
            }

            CharPos += useSiz * size + sizCalc;

            lastL = chData.getSize();

            RenderData.TextureY1 = 1.0 - (sizeOfLine * chData.getline());
            RenderData.TextureY2 = RenderData.TextureY1 - sizeOfLine;

            RenderData.TextureX1 = (positionFromCharInTexture * sizeFromChar);
            RenderData.TextureX2 =
                (positionFromCharInTexture * sizeFromChar) + sizeFromChar;

            engine.Render2DQuad(RenderData);
        }

        ++i;
    }
}

auto CFonts::addTextureToFont(const std::string &fontName,
                              const std::string &path,
                              const std::string &texture,
                              const std::wstring &textChars) -> std::string {
    auto &font = fontsReg[fontName];

    font.registerTexture(path, texture, textChars);

    return (path + "/" + texture);
}

auto CFonts::addTextureToFont(const std::string &fontName,
                              const std::string &path,
                              const std::string &texture,
                              const std::string &textChars) -> std::string {
    auto &font = fontsReg[fontName];

    font.registerTexture(path, texture, textChars);

    return (path + "/" + texture);
}

auto CFonts::getCenterPos(const std::string &text, double size, double posX1,
                          const std::string &fontName) -> double {
    return posX1 - getXSizeInScreen(text, size, fontName) / 2.0;
}

auto CFonts::getXSizeInScreen(const std::string &text, double size,
                              const std::string &fontName) -> double {
    auto &fontToUse = fontsReg[fontName];

    const double sizeDiv2_0 = size / 2.0;
    const double sizeDiv12_0 = size / 12.0;
    const double sizeDiv10_0 = size / 10.0;
    const double sizeDiv20_0 = size / 20.0;
    double CharPos = 0.0;

    auto &engine = CEngine::engine();

    try {

        for (auto it = text.begin(); it != text.end(); /***/) {
            auto ch = utf8::next(it, text.end());
            auto &chData = fontToUse.chars[ch];

            if (iswblank(ch) != 0) {
                CharPos += size / 1.5;
            } else if (chData.getText() != nullptr) {
                const auto &fontsTextData = *chData.getText();

                if (chData.getAlign() > 0.15) {
                    {
                        CharPos += -(chData.getAlign() * size) + sizeDiv10_0;
                    }
                }

                if (chData.getAlign() < 0.1) {
                    {
                        CharPos += sizeDiv20_0;
                    }
                }

                double sizCalc = sizeDiv12_0;
                double useSiz = chData.getSize();

                if (useSiz < 0.45) {
                    useSiz = 0.45;
                } else if (useSiz > 0.9) {
                    sizCalc = 0;
                }

                CharPos += useSiz * size + sizCalc;
            }
        }
    } catch (const std::exception &e) {
        CLog::log().multiRegister(
            "CFonts::getCenterPos exception <%0>, string \"%1\"", e, text);
    }

    return CharPos;
}

CFonts::Font::Font() {}

CFonts::CFonts() {
    addTextureToFont("default", "data/sprites", "FONT.tga",
                     "0123456789ABCDEFGHIJKLMNOPQRSTUV\nWXYZabcdefghijklmnopqrs"
                     "tuvwxyz.,\nÃÂÀÁÄÊÈÉËÎÌÍÏÕÔ\n?----\n---");
    // ÃÂÀÁÄÊÈÉËÎÌÍÏÕÔÓÒÖ\xFFÛ
}
