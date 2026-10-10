#pragma once
#ifndef _GUITAR_PP_CCONTROLS_h_
#define _GUITAR_PP_CCONTROLS_h_

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include <array>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>

class CControls {
    CControls(CControls &) = delete;
    static void joystickCb(int jid, int eventId);
    int inited;
    int escCB{};
    static void key_callback(GLFWwindow *window, int key, int scancode,
                             int action, int mods);

    int lastJID, lastJEvent;

  public:
    static CControls &controls();

    enum JOYSTICK_STATE {
        JOYSTICK_CONNECTED = GLFW_CONNECTED,
        JOYSTICK_DISCONNECTED = GLFW_DISCONNECTED
    };

    enum class MenuAction { Up, Down, Left, Right, Confirm, Back, Count };
    struct MenuEvent {
        enum Kind { Key, Character, Mouse, Scroll, Focus } kind = Key;
        int key = 0, action = 0, mods = 0;
        unsigned codepoint = 0;
        double x = 0, y = 0;
    };

    struct MenuBinding {
        std::string guid;
        enum Source { Button, Hat, Axis } source = Button;
        int index = 0, direction = 1;
        MenuAction action = MenuAction::Confirm;
    };

    bool menuInputActive = false;
    std::vector<MenuEvent> menuEvents;
    std::vector<MenuBinding> menuBindings;
    void queueMenuEvent(const MenuEvent &event);
    void pollMenuDevices();
    void resetMenuInput();
    // Pure repeat filter, also used by tests. One action per poll, no catch-up.
    static bool menuRepeat(bool down, bool &previous, double &next, double now,
                           bool repeat);

  private:
    std::array<bool, 6> menuDown{};
    std::array<double, 6> menuNext{};
    std::vector<bool> bindingDown, nextBindingDown;
    std::array<bool, 2> blockConfirm{{true, true}};

  public:
    unsigned int lastChar;
    std::function<void(unsigned int)> textCallback;
    std::function<void(int key, int scancode, int action, int mods)>
        keyCallback;
    std::function<void(int jid, int eventId)> joystickCallback;

    int keyEscape() {
        int r = escCB;
        escCB = 0;
        return r;
    };

    static const unsigned char *getJoystickButtons(int jid, int &count);
    static const unsigned char *getJoystickHats(int jid, int &count);
    static bool isJoystickPresent(int jid);
    static const char *getJoystickName(int jid);
    static bool isJoystickGamepad(int jid);
    static const char *getGamepadName(int jid);
    static int getGamepadState(int jid);

    static void init();
    void update();
    CControls();

    struct joystick {
        std::string identification;
        int numKeys;
        int numAnalogs;
    };

    struct keyState {
        double t;
        double data;
        bool pressed, lastFramePressed;

        inline keyState() {
            t = 0.0;
            data = 0.0;
            pressed = lastFramePressed = false;
        }
    };

    struct key {
        int device;
        int keyID;

        bool isPressed() const;
        key() { device = keyID = 0; }
    };

    static int getKeyboardKeyState(int keyID);

    std::map<int, keyState> keys;

    bool getKeyState(const key &k);
    double getAnalogKeyState(const key &k);

    class CPlayerControls {

      public:
        std::vector<int> keys;

        template <class Archive> void load(Archive &archive) { archive(keys); }

        template <class Archive> void save(Archive &archive) const {
            archive(keys);
        }

        CPlayerControls();
    };

    struct shortcutKeys {
        std::vector<key> keys;

        bool isPressed() const {
            for (auto &k : keys) {
                if (k.isPressed()) {
                    return true;
                }
            }

            return false;
        }
    };
};

#endif
