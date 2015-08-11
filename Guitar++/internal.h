#pragma once
#ifndef __GPP__GLFW2__INTELNAL_H__
#define __GPP__GLFW2__INTELNAL_H__

#ifndef GLFW_NO_RESCALE_BIT
#define GLFW_NO_RESCALE_BIT       0x00000001 /* Only for glfwReadImage */
#define GLFW_ORIGIN_UL_BIT        0x00000002
#define GLFW_BUILD_MIPMAPS_BIT    0x00000004 /* Only for glfwLoadTexture2D */
#define GLFW_ALPHA_MAP_BIT        0x00000008
#endif

#ifndef GL_TRUE
#define GL_TRUE 1
#define GL_FALSE 0
#endif

int _glfwReadTGA(CEngine::GLFWstream *s, CEngine::GLFWimage *img, int flags);

#endif
