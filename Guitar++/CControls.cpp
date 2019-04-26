#include "CControls.h"
#include "CEngine.h"
#include <GLFW/glfw3.h>
#include <iostream>

void character_callback(GLFWwindow* window, unsigned int codepoint)
{
	CControls::controls().lastChar = codepoint;
	if (CControls::controls().textCallback) CControls::controls().textCallback(codepoint);
}

void CControls::update()
{
	auto &engine = CEngine::engine();

	double time = engine.getTime();

	for (int i = 32; i <= GLFW_KEY_LAST; i++)
	{
		auto &k = keys[i];

		if (k.pressed)
		{
			k.lastFramePressed = true;
		}
		else
		{
			k.lastFramePressed = false;
		}

		k.pressed = engine.getKey(i);

		if (k.pressed && !k.lastFramePressed)
		{
			k.t = time;
		}
	}
}

CControls &CControls::controls()
{
	static CControls controls;
	return controls;
}


CControls::CControls()
{
	lastChar = 0;
	glfwSetCharCallback((GLFWwindow*)CEngine::engine().getWindow(), character_callback);


}


CControls::CPlayerControls::CPlayerControls()
{

}

