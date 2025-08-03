// Defines symbols used by the platform to input Input events to the client.

#ifndef SYNERGY_CLIENT_INPUT_INCLUDED
#define SYNERGY_CLIENT_INPUT_INCLUDED

#include "SynergyCore.h"

#include "SynergyClientViewport.h"

#include <stdint.h>

/*
    Simplified representation of supported keys for performing actions in the software. They include:
    - Alphanumerics (without case)
    - Functions keys
    - Modifier keys
    - Mouse buttons

    NOTE: This should NOT be used for typing text !
*/
enum class ActionKey : uint8_t
{
    ACTION_KEY_NONE, // 0 = None so Action Input Event objects are invalid when initialized from zeroed out memory.
    ACTION_KEYS_START,
    NUMBERS_START = ACTION_KEYS_START,
    KEY_0 = NUMBERS_START,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    NUMBERS_END = KEY_9,
    LETTERS_START,
    KEY_A = LETTERS_START,
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,
    ALPHANUMERIC_LETTERS_END = KEY_Z,
    FUNCTION_KEYS_START,
    KEY_FUNC1 = FUNCTION_KEYS_START,
    KEY_FUNC2,
    KEY_FUNC3,
    KEY_FUNC4,
    KEY_FUNC5,
    KEY_FUNC6,
    KEY_FUNC7,
    KEY_FUNC8,
    KEY_FUNC9,
    KEY_FUNC10,
    KEY_FUNC11,
    KEY_FUNC12,
    FUNCTION_KEYS_END = KEY_FUNC12,
    ARROW_KEYS_START,
    ARROW_LEFT = ARROW_KEYS_START,
    ARROW_UP,
    ARROW_RIGHT,
    ARROW_DOWN,
    ARROW_KEYS_END = ARROW_RIGHT,
    MODIFIER_KEYS_START,
    MOD_CTRL = MODIFIER_KEYS_START,
    MOD_SHIFT,
    MOD_ALT,
    MODIFIER_KEYS_END = MOD_ALT,
    MOUSE_BUTTONS_START,
    MOUSE_LEFT = MOUSE_BUTTONS_START,
    MOUSE_RIGHT,
    MOUSE_MIDDLE,
    MOUSE_BUTTONS_END,
    ACTION_KEYS_END = MOUSE_BUTTONS_END,
    ACTION_KEY_COUNT
};

/*
    Input event usable by the Client as a way to change the state of the program through the use of a keyboard or mouse button.
*/
struct ActionInputEvent
{
    // Time this event happened at normalized from Frame Start Time - Frame Time to Frame Start Time.
    float TimeNormalized;
    
    // Viewport this event relates to.
    ViewportID Viewport;

    // Key or Button associated with the event.
    ActionKey Key;

    // If true, then this is the Release of the Action Input Event. If false, it's the start.
    bool bRelease;
};

#endif
