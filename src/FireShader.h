#pragma once
#include <string_view>

// GLSL 1.10: reuse the compatibility renderer's matrices and client arrays.
namespace FireShader {
inline constexpr std::string_view vertex = R"glsl(#version 110
varying vec2 uv;
varying vec4 color;
varying float phase;
void main() {
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    uv = gl_MultiTexCoord0.xy;
    color = gl_Color;
    phase = gl_Vertex.x * 19.0;
}
)glsl";

inline constexpr std::string_view fragment = R"glsl(#version 110
uniform float uTime;
uniform float uPlusActive;
uniform float uMultiplier;
uniform float uPublicApproval;
varying vec2 uv;
varying vec4 color;
varying float phase;
float flameNoise(vec2 p) {
    vec2 cell = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    vec4 corners = vec4(dot(cell, vec2(127.1, 311.7)))
                 + vec4(0.0, 127.1, 311.7, 438.8);
    vec4 noise = fract(sin(corners) * 43758.5453);
    return mix(mix(noise.x, noise.y, f.x),
               mix(noise.z, noise.w, f.x), f.y);
}

void main() {
    // The existing particle quads have v=0 at the top.
    // Grow within the compact particle bounds; Plus multipliers cap at 4x.
    float strength = (clamp(uMultiplier, 1.0, 4.0) - 1.0) / 3.0;
    float height = (1.0 - uv.y) / mix(0.65, 1.0, strength);
    float t = uTime * 7.0 + phase;
    float sway = (sin(height * 9.0 - t) * 0.07
                + sin(height * 17.0 - t * 1.7) * 0.03) * height;
    float x = (uv.x - 0.5) * 2.0 + sway;
    float h = clamp(height, 0.0, 1.0);
    // A broad, rounded body with several moving tongues instead of one cone.
    // Two fixed noise scales add rising wisps without textures or extra quads.
    float width = (0.94 - 0.30 * h - 0.30 * h * h)
                * mix(0.80, 1.0, strength);
    float billow = flameNoise(vec2(x * 6.0, height * 7.0 - t));
    float detail = flameNoise(vec2(x * 17.0, height * 19.0 - t * 2.0));
    float ripple = billow * 0.7 + detail * 0.3;
    float body = 1.0 - smoothstep(width * 0.65, width,
                                abs(x) + (ripple - 0.5) * 0.30 * h);
    float crown = 0.76 + 0.12 * sin(x * 19.0 + t * 0.8)
                       + 0.06 * sin(x * 37.0 - t * 1.3);
    crown += (billow - 0.5) * 0.20;
    float tip = 1.0 - smoothstep(crown - 0.16, crown, height);
    float base = smoothstep(0.0, 0.055, height);
    float heat = clamp(body * (1.20 - 0.55 * h)
                       + (ripple - 0.5) * 0.9 * h, 0.0, 1.0);
    float disapproval = 1.0 - clamp(uPublicApproval, 0.0, 1.0);
    vec3 outerColor = mix(vec3(1.0, 0.48, 0.015), vec3(1.0, 0.015, 0.005), disapproval);
    vec3 coreColor = mix(vec3(1.0, 0.98, 0.82), vec3(1.0, 0.22, 0.04), disapproval);
    outerColor = mix(outerColor, vec3(0.02, 0.18, 1.0), uPlusActive);
    coreColor = mix(coreColor, vec3(0.25, 0.90, 1.0), uPlusActive);
    vec3 flame = mix(outerColor, coreColor, heat);
    float alpha = body * tip * base * color.a * mix(0.45, 1.0, strength);
    gl_FragColor = vec4(flame * color.rgb, alpha);
}
)glsl";
} // namespace FireShader
