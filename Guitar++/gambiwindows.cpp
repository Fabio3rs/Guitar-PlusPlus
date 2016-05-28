#include "gambiwindows.h"
#include <Windows.h>
#include <injector\hooking.hpp>

void startGambiarras()
{
	HMODULE ogl = LoadLibraryA("opengl32.dll");
	FARPROC wglsp = GetProcAddress(ogl, "wglSwapBuffers");

	HMODULE gdi = LoadLibraryA("gdi32.dll");
	FARPROC gdilsp = GetProcAddress(gdi, "SwapBuffers");

	injector::MakeJMP(gdilsp, wglsp);
}
