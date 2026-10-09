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
varying vec2 uv;
varying vec4 color;
varying float phase;
void main() {
    // The existing particle quads have v=0 at the top.
    float height = 1.0 - uv.y;
    float t = uTime * 7.0 + phase;
    float sway = (sin(height * 9.0 - t) * 0.12
                + sin(height * 17.0 - t * 1.7) * 0.05) * height;
    float x = (uv.x - 0.5) * 2.0 + sway;
    float width = mix(0.85, 0.04, height);
    float body = 1.0 - smoothstep(width * 0.45, width, abs(x));
    float tip = 1.0 - smoothstep(0.70, 1.0, height);
    float base = smoothstep(0.0, 0.10, height);
    float heat = (1.0 - height) * body;
    vec3 flame = mix(vec3(1.0, 0.12, 0.01), vec3(1.0, 0.90, 0.25), heat);
    float alpha = body * tip * base * color.a;
    gl_FragColor = vec4(flame * color.rgb, alpha);
}
)glsl";
} // namespace FireShader
