// Platform doubles for the production methods injected by test_gamepad_neutral.py.
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <stdexcept>
#include <unordered_map>
#include <vector>

enum class GamepadAxisId { LEFT_X, LEFT_Y, RIGHT_X, RIGHT_Y, LEFT_TRIGGER, RIGHT_TRIGGER };
enum class GamepadButtonId { DPAD_UP, DPAD_DOWN, DPAD_LEFT, DPAD_RIGHT };
enum { AINPUT_SOURCE_GAMEPAD, AMOTION_EVENT_ACTION_MOVE,
       AMOTION_EVENT_AXIS_X, AMOTION_EVENT_AXIS_Y, AMOTION_EVENT_AXIS_RX,
       AMOTION_EVENT_AXIS_RY, AMOTION_EVENT_AXIS_BRAKE, AMOTION_EVENT_AXIS_GAS,
       AMOTION_EVENT_AXIS_HAT_X, AMOTION_EVENT_AXIS_HAT_Y };
struct GameActivityMotionEvent {
    int source, deviceId, action, pointerCount;
    struct Pointer { int id; float axisValues[64]; } pointers[1];
};
struct FakeMotionEvent {
    std::function<float(int)> axisFunction;
    FakeMotionEvent(int, int, int, int, float, float, std::function<float(int)> callback)
        : axisFunction(callback) {}
};
struct Window {
    std::vector<bool> cursorChanges;
    void setCursorDisabled(bool disabled) { cursorChanges.push_back(disabled); }
};
struct JniSupport {
    bool gameActivity = false;
    std::vector<GameActivityMotionEvent> events;
    bool isGameActivityVersion() { return gameActivity; }
    void sendMotionEvent(GameActivityMotionEvent* event) { events.push_back(*event); }
};
struct InputQueue {
    std::vector<FakeMotionEvent> events;
    void addEvent(FakeMotionEvent event) { events.push_back(event); }
};
struct WindowCallbacks {
    enum class InputMode { Touch, Mouse, Gamepad, Unknown };
    struct GamepadData { float axis[6] = {}; bool button[15] = {}; };
    Window window;
    JniSupport jniSupport;
    InputQueue inputQueue;
    std::unordered_map<int, GamepadData> gamepads{{1, {}}};
    bool sendEvents = true, useRawInput = false, cursorLocked = false;
    bool useMixedInput = false;
    bool needsQueueGamepadInput = true;
    InputMode inputMode = InputMode::Unknown, forcedMode = InputMode::Unknown;
    int inputModeSwitchDelay = 100;
    std::chrono::high_resolution_clock::time_point lastUpdated;
    bool hasInputMode(InputMode want, bool changeMode = true);
    void setCursorLocked(bool locked);
    void queueGamepadAxisInputIfNeeded(int gamepad);
    void onGamepadAxis(int gamepad, GamepadAxisId axis, float value);
};

// PRODUCTION_METHODS

using Mode = WindowCallbacks::InputMode;
constexpr int androidAxes[] = {AMOTION_EVENT_AXIS_X, AMOTION_EVENT_AXIS_Y,
    AMOTION_EVENT_AXIS_RX, AMOTION_EVENT_AXIS_RY, AMOTION_EVENT_AXIS_BRAKE, AMOTION_EVENT_AXIS_GAS};

int main() {
    int scenarios = 0;
    for(bool activity : {false, true}) {
        for(int delay : {0, 100}) {
            for(int axis = 0; axis < 6; ++axis) {
                WindowCallbacks callbacks;
                callbacks.jniSupport.gameActivity = activity;
                callbacks.inputModeSwitchDelay = delay;
                auto id = static_cast<GamepadAxisId>(axis);
                callbacks.onGamepadAxis(1, id, 0.8f);
                assert(callbacks.gamepads.at(1).axis[axis] == 0.8f);
                assert(callbacks.hasInputMode(Mode::Mouse));
                auto cursorChanges = callbacks.window.cursorChanges.size();
                // Legacy queue is still pending: its callback must see the release.
                callbacks.onGamepadAxis(1, id, 0.f);
                assert(callbacks.gamepads.at(1).axis[axis] == 0.f);
                assert(callbacks.inputMode == Mode::Mouse);
                assert(callbacks.window.cursorChanges.size() == cursorChanges);
                if(activity) {
                    assert(callbacks.jniSupport.events.size() == 2);
                    assert(callbacks.jniSupport.events.back().pointers[0].axisValues[androidAxes[axis]] == 0.f);
                } else {
                    assert(callbacks.inputQueue.events.size() == 1);
                    assert(callbacks.inputQueue.events.back().axisFunction(androidAxes[axis]) == 0.f);
                }
                // A second neutral input must not create another gamepad event.
                auto count = callbacks.jniSupport.events.size();
                callbacks.needsQueueGamepadInput = true;
                callbacks.onGamepadAxis(1, id, 0.f);
                assert(callbacks.jniSupport.events.size() == count);
                assert(activity || callbacks.inputQueue.events.size() == 1);
                ++scenarios;
            }
        }
        // Delivery after the legacy queue has already been consumed.
        WindowCallbacks callbacks;
        callbacks.jniSupport.gameActivity = activity;
        callbacks.onGamepadAxis(1, GamepadAxisId::LEFT_X, 0.9f);
        callbacks.onGamepadAxis(1, GamepadAxisId::LEFT_Y, -0.9f);
        assert(callbacks.hasInputMode(Mode::Mouse));
        callbacks.inputQueue.events.clear();
        callbacks.needsQueueGamepadInput = true;
        callbacks.onGamepadAxis(1, GamepadAxisId::LEFT_X, 0.f);
        callbacks.onGamepadAxis(1, GamepadAxisId::LEFT_Y, 0.f);
        if(activity) {
            const auto& event = callbacks.jniSupport.events.back();
            assert(event.pointers[0].axisValues[AMOTION_EVENT_AXIS_X] == 0.f);
            assert(event.pointers[0].axisValues[AMOTION_EVENT_AXIS_Y] == 0.f);
        } else {
            assert(callbacks.inputQueue.events.size() == 1);
            for(auto axis : {AMOTION_EVENT_AXIS_X, AMOTION_EVENT_AXIS_Y})
                assert(callbacks.inputQueue.events.back().axisFunction(axis) == 0.f);
            // An old queued event returns neutral after disconnect.
            callbacks.gamepads.erase(1);
            assert(callbacks.inputQueue.events.back().axisFunction(AMOTION_EVENT_AXIS_X) == 0.f);
            callbacks.gamepads.emplace(1, WindowCallbacks::GamepadData{});
            assert(callbacks.inputQueue.events.back().axisFunction(AMOTION_EVENT_AXIS_X) == 0.f);
        }
        ++scenarios;
    }
    for(bool startup : {false, true}) {
        WindowCallbacks callbacks;
        callbacks.gamepads.at(1).axis[0] = 0.8f;
        callbacks.inputMode = Mode::Mouse;
        callbacks.sendEvents = !startup;
        callbacks.forcedMode = startup ? Mode::Unknown : Mode::Mouse;
        callbacks.onGamepadAxis(1, GamepadAxisId::LEFT_X, 0.f);
        assert(callbacks.gamepads.at(1).axis[0] == 0.8f);
        assert(callbacks.inputQueue.events.empty());
        ++scenarios;
    }
    for(bool raw : {false, true}) {
        WindowCallbacks callbacks;
        callbacks.useRawInput = raw;
        callbacks.forcedMode = raw ? Mode::Mouse : Mode::Gamepad;
        callbacks.onGamepadAxis(1, GamepadAxisId::LEFT_X, 0.2f);
        assert(callbacks.gamepads.at(1).axis[0] == 0.2f);
        callbacks.onGamepadAxis(1, GamepadAxisId::LEFT_X, 0.f);
        assert(callbacks.gamepads.at(1).axis[0] == 0.f);
        ++scenarios;
    }
    WindowCallbacks callbacks;
    callbacks.inputMode = Mode::Mouse;
    callbacks.lastUpdated = std::chrono::high_resolution_clock::now();
    callbacks.inputModeSwitchDelay = 100000;
    for(float value : {0.2f, -0.2f, 0.9f}) {
        callbacks.onGamepadAxis(1, GamepadAxisId::LEFT_X, value);
        assert(callbacks.gamepads.at(1).axis[0] == 0.f);
        assert(callbacks.inputQueue.events.empty());
    }
    callbacks.onGamepadAxis(42, GamepadAxisId::LEFT_X, 0.f);
    assert(callbacks.gamepads.size() == 1);
    bool threw = false;
    try { callbacks.onGamepadAxis(1, static_cast<GamepadAxisId>(6), 0.f); }
    catch(const std::runtime_error&) { threw = true; }
    assert(threw);
    for(bool activity : {false, true}) {
        WindowCallbacks mixed;
        mixed.jniSupport.gameActivity = activity;
        mixed.useMixedInput = true;
        mixed.inputModeSwitchDelay = 100000;
        mixed.setCursorLocked(true);
        for(int round = 0; round < 3; ++round) {
            mixed.onGamepadAxis(1, GamepadAxisId::LEFT_X, 0.25f);
            assert(mixed.gamepads.at(1).axis[0] == 0.25f);
            assert(mixed.inputMode == Mode::Gamepad);
            // Even small relative mouse motion must be accepted while captured.
            assert(mixed.hasInputMode(Mode::Mouse, false));
            mixed.onGamepadAxis(1, GamepadAxisId::LEFT_X, 0.f);
            assert(mixed.gamepads.at(1).axis[0] == 0.f);
            assert(mixed.inputMode == Mode::Gamepad);
            mixed.setCursorLocked(false);
            assert(mixed.inputMode == Mode::Mouse);
            assert(!mixed.window.cursorChanges.back());
            // In menus the normal gamepad delay applies again.
            assert(!mixed.hasInputMode(Mode::Gamepad));
            assert(mixed.hasInputMode(Mode::Mouse));
            mixed.setCursorLocked(true);
            assert(mixed.window.cursorChanges.back());
        }
        mixed.sendEvents = false;
        assert(!mixed.hasInputMode(Mode::Gamepad));
        mixed.sendEvents = true;
        for(auto forced : {Mode::Touch, Mode::Mouse, Mode::Gamepad}) {
            mixed.forcedMode = forced;
            for(auto requested : {Mode::Touch, Mode::Mouse, Mode::Gamepad})
                assert(mixed.hasInputMode(requested) == (requested == forced));
        }
        mixed.forcedMode = Mode::Unknown;
        mixed.inputMode = Mode::Touch;
        mixed.lastUpdated = std::chrono::high_resolution_clock::now();
        assert(!mixed.hasInputMode(Mode::Mouse));
        assert(!mixed.hasInputMode(Mode::Gamepad));
        ++scenarios;
    }
    std::printf("Passed %d release/guard scenarios plus filtering and invalid-device checks.\n", scenarios);
}
