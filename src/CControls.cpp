#include "CControls.h"
#include "CEngine.h"
#include <iostream>

static void character_callback(GLFWwindow *window, unsigned int codepoint) {
    auto &inst = CControls::controls();
    inst.lastChar = codepoint;
    if (inst.textCallback) {
        {
            inst.textCallback(codepoint);
        }
    }
}

void CControls::joystickCb(int jid, int eventId) {
    auto &inst = controls();
    inst.lastJID = jid;
    inst.lastJEvent = eventId;

    if (inst.joystickCallback) {
        {
            inst.joystickCallback(jid, eventId);
        }
    }
}

void CControls::key_callback(GLFWwindow *window, int key, int scancode,
                             int action, int mods) {
    auto &inst = CControls::controls();

    auto &k = inst.keys[key];

    bool press = (action == GLFW_PRESS);
    bool repeat = (action == GLFW_REPEAT);

    k.pressed = press || repeat;
    k.lastFramePressed = (action == GLFW_REPEAT);

    if (key == GLFW_KEY_ESCAPE) {
        {
            inst.escCB = static_cast<int>(action == GLFW_PRESS);
        }
    }

    if (inst.keyCallback) {
        {
            inst.keyCallback(key, scancode, action, mods);
        }
    }
}

auto CControls::getJoystickButtons(int jid, int &count) -> const
    unsigned char * {
    return glfwGetJoystickButtons(jid, &count);
}

auto CControls::getJoystickHats(int jid, int &count) -> const unsigned char * {
    return glfwGetJoystickHats(jid, &count);
}

auto CControls::isJoystickPresent(int jid) -> bool {
    return glfwJoystickPresent(jid) != 0;
}

auto CControls::getJoystickName(int jid) -> const char * {
    return glfwGetJoystickName(jid);
}

auto CControls::isJoystickGamepad(int jid) -> bool {
    return glfwJoystickIsGamepad(jid) != 0;
}

auto CControls::getGamepadName(int jid) -> const char * {
    return glfwGetGamepadName(jid);
}

auto CControls::getGamepadState(int jid) -> int {
    return 0 /*glfwGetGamepadState(jid)*/;
}

void CControls::init() {
    glfwSetCharCallback((GLFWwindow *)CEngine::engine().getWindow(),
                        character_callback);
    glfwSetKeyCallback((GLFWwindow *)CEngine::engine().getWindow(),
                       key_callback);
    glfwSetJoystickCallback(joystickCb);
}

void CControls::update() {}

auto CControls::controls() -> CControls & {
    static CControls controls;
    return controls;
}

CControls::CControls() {
    lastChar = 0;
    inited = 0;
    lastJID = lastJEvent = 0;
}

auto CControls::getKeyboardKeyState(int keyID) -> int {
    return CEngine::engine().getKey(keyID);
}

CControls::CPlayerControls::CPlayerControls() {}

auto CControls::key::isPressed() const -> bool {
    if (device == 0) {
        return CEngine::engine().getKey(keyID) != 0;
    }
    return false;
}
