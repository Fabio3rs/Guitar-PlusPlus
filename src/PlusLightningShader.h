#pragma once
#include <string_view>

namespace PlusLightningShader {
inline constexpr std::string_view vertex = R"glsl(#version 110
varying vec2 uv;

void main() {
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    uv = gl_MultiTexCoord0.xy;
}

)glsl";

inline constexpr std::string_view fragment = R"glsl(#version 110
uniform float uTime;
uniform float uOpacity;
uniform float uSpark;
varying vec2 uv;

float hash(float n) { return fract(sin(n * 127.1) * 43758.5453); }

float path(float y, float tick) {
    float segment = y * 18.0;
    float index = floor(segment);
    float jitter = mix(hash(index + tick * 31.0),
                       hash(index + 1.0 + tick * 31.0), fract(segment));
    return 0.5 + (jitter - 0.5) * 0.32 * sin(y * 3.14159265);
}

void main() {
    if (uSpark > 0.5) {
        vec2 delta = (uv - vec2(0.5)) * 2.0;
        float alpha = (1.0 - smoothstep(0.0, 1.0, dot(delta, delta))) * uOpacity;
        gl_FragColor = vec4(0.5, 0.85, 1.0, alpha);
        return;
    }
    float y = 1.0 - uv.y;
    float tick = floor(uTime * 22.0);
    float center = path(y, tick);
    float distanceToBolt = abs(uv.x - center);
    float core = 1.0 - smoothstep(0.004, 0.016, distanceToBolt);
    float glow = exp(-distanceToBolt * 48.0) * 0.65;
    float forkCenter = center + (y - 0.35) * 0.45;
    float fork = (1.0 - smoothstep(0.003, 0.009, abs(uv.x - forkCenter)))
               * smoothstep(0.33, 0.39, y) * (1.0 - smoothstep(0.60, 0.75, y));
    float ends = smoothstep(0.0, 0.025, y) * (1.0 - smoothstep(0.95, 1.0, y));
    float energy = core + glow + fork * 0.65;
    vec3 tint = mix(vec3(0.12, 0.45, 1.0), vec3(0.8, 0.95, 1.0), core);
    gl_FragColor = vec4(tint, clamp(energy * ends * uOpacity, 0.0, 1.0));
}
)glsl";
} // namespace PlusLightningShader
