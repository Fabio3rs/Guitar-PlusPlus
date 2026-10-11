#pragma once
#include <string_view>

namespace BackgroundShader {

inline constexpr std::string_view vertex = R"glsl(#version 110
uniform mat4 uLightMatrix;
varying vec3 worldPosition;
varying vec3 normal;
varying vec2 uv;
varying vec2 aoUV;
varying vec4 shadowPosition;
void main() {
    // CEngine places the view transform in GL_PROJECTION. MODELVIEW contains
    // only the object's transform; lighting must therefore use world space.
    vec4 world=gl_ModelViewMatrix*gl_Vertex;
    worldPosition=world.xyz;
    normal=gl_NormalMatrix*gl_Normal;
    uv=gl_MultiTexCoord0.xy;
    aoUV=gl_MultiTexCoord1.xy;
    shadowPosition=uLightMatrix*world;
    gl_Position=gl_ModelViewProjectionMatrix*gl_Vertex;
}
)glsl";

inline constexpr std::string_view fragment = R"glsl(#version 110
uniform sampler2D uTexture;
uniform sampler2D uShadow;
uniform sampler2D uAO;
uniform bool uAOEnabled;
uniform vec3 uAmbient;
uniform bool uTextured;
uniform bool uShadows;
uniform vec2 uUVScale;
uniform vec2 uUVOffset;
uniform bool uClampUV;
uniform vec3 uDiffuse;
uniform vec3 uSpecular;
uniform vec3 uEmission;
uniform float uShininess;
uniform float uAlphaCutoff;
uniform float uOpacity;
uniform vec3 uEye;
uniform vec3 uLight;
uniform float uShadowTexel;
uniform float uShadowBias;
varying vec3 worldPosition;
varying vec3 normal;
varying vec2 uv;
varying vec2 aoUV;
varying vec4 shadowPosition;
float comparison(vec2 p,float depth) {
    return step(depth,texture2D(uShadow,p).r);
}
float visibility(float ndotl) {
    if(!uShadows || shadowPosition.w<=0.0) return 1.0;
    vec3 p=shadowPosition.xyz/shadowPosition.w*0.5+0.5;
    if(p.x<=0.0 || p.x>=1.0 || p.y<=0.0 || p.y>=1.0 || p.z<=0.0 || p.z>=1.0) return 1.0;
    float z=p.z-uShadowBias*(1.0+2.0*(1.0-ndotl));
    vec2 d=vec2(uShadowTexel*0.5,0.0);
    return 0.25*(comparison(p.xy+d.xx,z)+comparison(p.xy-d.xx,z)
                +comparison(p.xy+vec2(d.x,-d.x),z)+comparison(p.xy+vec2(-d.x,d.x),z));
}
void main() {
    vec2 coord=uv*uUVScale+uUVOffset;
    if(uClampUV) coord=clamp(coord,0.0,1.0);
    vec4 texel=vec4(1.0);
    if(uTextured) texel=texture2D(uTexture,coord);
    float alpha=texel.a*uOpacity;
    if(alpha<uAlphaCutoff) discard;
    vec3 n=normalize(normal);
    if(!gl_FrontFacing) n=-n;
    vec3 delta=uLight-worldPosition;
    float distance2=dot(delta,delta);
    vec3 l=normalize(delta);
    vec3 v=normalize(uEye-worldPosition);
    float diffuse=max(dot(n,l),0.0);
    float cone=smoothstep(0.12,0.32,l.y);
    float attenuation=2.2*cone/(1.0+0.065*distance2);
    float shadow=visibility(diffuse);
    vec3 ambient=mix(vec3(0.16,0.17,0.20),vec3(0.34,0.32,0.29),n.y*0.5+0.5);
    float highlight=0.0;
    if(diffuse>0.0) highlight=pow(max(dot(n,normalize(l+v)),0.0),max(uShininess,1.0));
    vec3 direct=(texel.rgb*uDiffuse*diffuse+uSpecular*highlight)*vec3(1.0,0.94,0.83)*attenuation*shadow;
    float ao=1.0;
    if(uAOEnabled) ao=mix(0.2,1.0,texture2D(uAO,aoUV).r);
    gl_FragColor=vec4(texel.rgb*uDiffuse*uAmbient*ambient*ao+direct+texel.rgb*uEmission,alpha);
}
)glsl";
inline constexpr std::string_view depthVertex = R"glsl(#version 110
varying vec2 uv;
void main() { uv=gl_MultiTexCoord0.xy; gl_Position=gl_ModelViewProjectionMatrix*gl_Vertex; }
)glsl";
inline constexpr std::string_view depthFragment = R"glsl(#version 110
uniform sampler2D uTexture;
uniform bool uTextured;
uniform vec2 uUVScale;
uniform vec2 uUVOffset;
uniform bool uClampUV;
uniform float uOpacity;
varying vec2 uv;
void main() {
    vec2 coord=uv*uUVScale+uUVOffset;
    if(uClampUV) coord=clamp(coord,0.0,1.0);
    float alpha=uOpacity;
    if(uTextured) alpha*=texture2D(uTexture,coord).a;
    if(alpha<0.5) discard;
    gl_FragColor=vec4(1.0);
}
)glsl";
} // namespace BackgroundShader
