#include "CControls.h"
#include "CEngine.h"
#include <iostream>

static void character_callback(GLFWwindow *window, unsigned int codepoint) {
    auto &inst = CControls::controls();
    inst.lastChar = codepoint;
    CControls::MenuEvent event;
    event.kind = CControls::MenuEvent::Character;
    event.codepoint = codepoint;
    inst.queueMenuEvent(event);
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

    CControls::MenuEvent event;
    event.key = key;
    event.action = action;
    event.mods = mods;
    inst.queueMenuEvent(event);
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
    auto *window = static_cast<GLFWwindow *>(
        const_cast<void *>(CEngine::engine().getWindow()));
    glfwSetMouseButtonCallback(
        window, [](GLFWwindow *w, int button, int action, int mods) {
            int width, height;
            double x, y;
            glfwGetWindowSize(w, &width, &height);
            glfwGetCursorPos(w, &x, &y);
            if (width <= 0 || height <= 0) {
                return;
            }

            MenuEvent event;
            event.kind = MenuEvent::Mouse;
            event.key = button;
            event.action = action;
            event.mods = mods;
            event.x = (2 * x / width - 1) * static_cast<double>(width) / height;
            event.y = 1 - 2 * y / height;
            controls().queueMenuEvent(event);
        });
    glfwSetScrollCallback(window, [](GLFWwindow *, double x, double y) {
        MenuEvent event;
        event.kind = MenuEvent::Scroll;
        event.x = x;
        event.y = y;
        controls().queueMenuEvent(event);
    });
    glfwSetWindowFocusCallback(window, [](GLFWwindow *, int focused) {
        MenuEvent event;
        event.kind = MenuEvent::Focus;
        event.action = focused;
        controls().queueMenuEvent(event);
        if (!focused) {
            controls().menuDown.fill(false);
            controls().bindingDown.clear();
        }
    });
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

void CControls::queueMenuEvent(const MenuEvent &event) {
    if (!menuInputActive) {
        return;
    }

    if (menuEvents.size() >= 4096) {
        // Cancel captures rather than leave a virtual button/fret held on
        // overflow.
        menuEvents.clear();
        MenuEvent cancel;
        cancel.kind = MenuEvent::Focus;
        cancel.action = 0;
        menuEvents.push_back(cancel);
        return;
    }

    menuEvents.push_back(event);
}

void CControls::resetMenuInput() {
    menuEvents.clear();
    menuDown.fill(false);
    menuNext.fill(0);
    bindingDown.clear();
    blockConfirm.fill(true);
}

bool CControls::menuRepeat(bool down, bool &previous, double &next, double now,
                           bool repeat) {
    const bool fire = down && (!previous || (repeat && now >= next));
    if (fire) {
        next = now + (previous ? .075 : .35);
    }

    previous = down;
    return fire;
}

void CControls::pollMenuDevices() {
    if (!menuInputActive || !CEngine::engine().getWindow()) {
        return;
    }

    auto *window = static_cast<GLFWwindow *>(
        const_cast<void *>(CEngine::engine().getWindow()));
    if (!glfwGetWindowAttrib(window, GLFW_FOCUSED)) {
        menuDown.fill(false);
        bindingDown.clear();
        blockConfirm.fill(true);
        return;
    }

    std::array<bool, 6> down{};
    if (bindingDown.size() != menuBindings.size()) {
        bindingDown.assign(menuBindings.size(), false);
    }

    nextBindingDown.assign(menuBindings.size(), false);
    for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid) {
        if (!glfwJoystickPresent(jid)) {
            continue;
        }

        GLFWgamepadstate state{};
        if (glfwGetGamepadState(jid, &state)) {
            down[0] = down[0] || state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] ||
                      state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] <
                          (menuDown[0] ? -.35f : -.55f);
            down[1] = down[1] || state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] ||
                      state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] >
                          (menuDown[1] ? .35f : .55f);
            down[2] = down[2] || state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT] ||
                      state.axes[GLFW_GAMEPAD_AXIS_LEFT_X] <
                          (menuDown[2] ? -.35f : -.55f);
            down[3] = down[3] ||
                      state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] ||
                      state.axes[GLFW_GAMEPAD_AXIS_LEFT_X] >
                          (menuDown[3] ? .35f : .55f);
            down[4] = down[4] || state.buttons[GLFW_GAMEPAD_BUTTON_A];
            down[5] = down[5] || state.buttons[GLFW_GAMEPAD_BUTTON_B];
        }

        const char *guid = glfwGetJoystickGUID(jid);
        for (size_t i = 0; i < menuBindings.size(); ++i) {
            const auto &b = menuBindings[i];
            if (!guid || b.guid != guid) {
                continue;
            }

            int count = 0;
            bool pressed = false;
            if (b.source == MenuBinding::Button) {
                const auto *values = glfwGetJoystickButtons(jid, &count);
                pressed = values && b.index >= 0 && b.index < count &&
                          values[b.index];
            } else if (b.source == MenuBinding::Hat) {
                const auto *values = glfwGetJoystickHats(jid, &count);
                pressed = values && b.index >= 0 && b.index < count &&
                          (values[b.index] & b.direction);
            } else {
                const auto *values = glfwGetJoystickAxes(jid, &count);
                pressed = values && b.index >= 0 && b.index < count &&
                          values[b.index] * b.direction >
                              (bindingDown[i] ? .35f : .55f);
            }

            nextBindingDown[i] = nextBindingDown[i] || pressed;
            down[static_cast<size_t>(b.action)] =
                down[static_cast<size_t>(b.action)] || pressed;
        }
    }

    bindingDown.swap(nextBindingDown);
    for (size_t i = 0; i < blockConfirm.size(); ++i) {
        if (!down[i + 4]) {
            blockConfirm[i] = false;
        }

        if (blockConfirm[i]) {
            down[i + 4] = false;
        }
    }

    const int keys[] = {GLFW_KEY_UP,    GLFW_KEY_DOWN,  GLFW_KEY_LEFT,
                        GLFW_KEY_RIGHT, GLFW_KEY_ENTER, GLFW_KEY_ESCAPE};
    const double now = CEngine::engine().getTime();
    for (size_t i = 0; i < down.size(); ++i) {
        if (menuRepeat(down[i], menuDown[i], menuNext[i], now, i < 4)) {
            MenuEvent event;
            event.key = keys[i];
            event.action = GLFW_PRESS;
            queueMenuEvent(event);
        }
    }
}
