#include "CParticle.h"
#include "CEngine.h"

void CParticle::addParticle(const particleData &pdata) {
    part.push_front(pdata);

    part.front().addedAt = CEngine::engine().getTime();
}

void CParticle::render() {
    partDrawData.clear();
    partDrawData.autoEnDisaColors = true;
    partDrawData.useColors = true;
    double deltaTime = CEngine::engine().getDeltaTime();
    double time = CEngine::engine().getTime();

    CEngine::RenderDoubleStruct pstruct{};

    pstruct.TextureX1 = 0.0;
    pstruct.TextureX2 = 1.0;

    pstruct.TextureY1 = 0.0;
    pstruct.TextureY2 = 1.0;

    for (auto &p : part) {
        if ((time - p.addedAt) > p.duration) {
            continue;
        }
        partDrawData.texture = p.texture;

        p.ax -= p.desac * deltaTime;
        p.ay -= p.desac * deltaTime;
        p.az -= p.desac * deltaTime;

        p.sx += p.ax * deltaTime;
        p.sy += p.ay * deltaTime;
        p.sz += p.az * deltaTime;

        p.x += p.sx * deltaTime;
        p.y += p.sy * deltaTime;
        p.z += p.sz * deltaTime;

        pstruct.x1 = p.x;
        pstruct.x2 = p.x + p.sizex;
        pstruct.x3 = p.x + p.sizex;
        pstruct.x4 = p.x;

        pstruct.y1 = p.y + p.sizey;
        pstruct.y2 = p.y + p.sizey;
        pstruct.y3 = p.y;
        pstruct.y4 = p.y;

        pstruct.z1 = p.z + p.sizez;
        pstruct.z2 = p.z + p.sizez;
        pstruct.z3 = p.z;
        pstruct.z4 = p.z;

        double alpha = (time - p.addedAt) / p.duration;

        pstruct.alphaTop = alpha;
        pstruct.alphaBottom = alpha;

        CEngine::pushQuad(partDrawData, pstruct);
    }

    if (!partDrawData.vArray.empty()) {
        {
            CEngine::engine().drawTrianglesWithAlpha(partDrawData);
        }
    }

    for (auto it = part.begin(); it != part.end(); /******/) {
        auto &itinst = (*it);
        if ((time - itinst.addedAt) <= itinst.duration) {
            ++it;
        } else {
            it = part.erase(it);
        }
    }
}

CParticle::CParticle() {}
