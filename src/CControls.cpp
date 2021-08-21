#include "CControls.h"
#include "CEngine.h"
#include <iostream>

void character_callback(GLFWwindow* window, unsigned int codepoint)
{
	auto &inst = CControls::controls();
	inst.lastChar = codepoint;
	if (inst.textCallback) inst.textCallback(codepoint);
}

void CControls::joystickCb(int jid, int eventId)
{
	auto &inst = controls();
	inst.lastJID = jid;
	inst.lastJEvent = eventId;

	if (inst.joystickCallback)
		inst.joystickCallback(jid, eventId);
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

const unsigned char *CControls::getJoystickButtons(int jid, int &count) const
{
	return glfwGetJoystickButtons(jid, &count);
}

const unsigned char *CControls::getJoystickHats(int jid, int &count) const
{
	return glfwGetJoystickHats(jid, &count);
}

bool CControls::isJoystickPresent(int jid) const
{
	return glfwJoystickPresent(jid);
}

const char *CControls::getJoystickName(int jid) const
{
	return glfwGetJoystickName(jid);
}

bool CControls::isJoystickGamepad(int jid) const
{
	return glfwJoystickIsGamepad(jid);
}

const char *CControls::getGamepadName(int jid) const
{
	return glfwGetGamepadName(jid);
}

int CControls::getGamepadState(int jid) const
{
	return 0/*glfwGetGamepadState(jid)*/;
}

void CControls::init()
{
	glfwSetCharCallback((GLFWwindow*)CEngine::engine().getWindow(), character_callback);
	glfwSetKeyCallback((GLFWwindow*)CEngine::engine().getWindow(), key_callback);
	glfwSetJoystickCallback(joystickCb);
}

void CControls::update()
{

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
	lastJID = lastJEvent = 0;
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
