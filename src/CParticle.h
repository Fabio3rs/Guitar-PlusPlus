#pragma once
#ifndef _CPARTICLE_H_
#define _CPARTICLE_H_
#include "CEngine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <string>
#include <utility>
#include <vector>

class CParticle {
    std::array<double, 5> nextFireEmission{};

  public:
    struct particleData {
        int qtd;
        double x, y, z, ax, ay, az, sx, sy, sz, desac, duration, addedAt, sizex,
            sizey, sizez;
        unsigned int texture;
        // Crossed flames use x/z as their center, rather than a quad corner.
        bool crossedFlame = false;
        int emitter = -1;
        double emitterHeight = 0.0;
    };
    std::deque<particleData> part;
    CEngine::dTriangleWithAlpha partDrawData;

    void addParticle(const particleData &pdata);
    // At most 30 emissions/s and eight live particles per lane, no catch-up
    // burst.
    bool emitFire(const particleData &pdata, double now);
    void moveEmitter(int emitter, double height);
    void render();
    void renderFire();

    CParticle();
};

#endif