#include "CControls.h"
#include "CEngine.h"
#include <iostream>

void character_callback(GLFWwindow* window, unsigned int codepoint)
{
	CControls::controls().lastChar = codepoint;
	if (CControls::controls().textCallback) CControls::controls().textCallback(codepoint);
}

void CControls::key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	auto &inst = CControls::controls();

	auto &k = inst.keys[key];

	bool press = (action == GLFW_PRESS);
	bool repeat = (action == GLFW_REPEAT);

	k.pressed = press || repeat;
	k.lastFramePressed = (action == GLFW_REPEAT);

	if (key == GLFW_KEY_ESCAPE)
		inst.escCB = (action == GLFW_PRESS);

	if (inst.keyCallback)
		inst.keyCallback(key, scancode, action, mods);
}

void CControls::init()
{
	glfwSetCharCallback((GLFWwindow*)CEngine::engine().getWindow(), character_callback);
	glfwSetKeyCallback((GLFWwindow*)CEngine::engine().getWindow(), key_callback);
	glfwSetJoystickCallback(joystickCb);
}

void CControls::update()
{
	/*auto &engine = CEngine::engine();

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
	}*/
}

void CControls::joystickCb(int jid, int eventId)
{
	std::cout << "joystick " << jid << " " << eventId << std::endl;
	// glfwGetGamepadState(jid, &state)
}

CControls &CControls::controls()
{
	static CControls controls;
	return controls;
}


CControls::CControls()
{
	lastChar = 0;
	inited = 0;
}

int CControls::getKeyboardKeyState(int keyID)
{
	return CEngine::engine().getKey(keyID);
}


CControls::CPlayerControls::CPlayerControls()
{

}

bool CControls::key::isPressed() const
{
	if (device == 0)
	{
		return CEngine::engine().getKey(keyID);
	}
	return false;
}
