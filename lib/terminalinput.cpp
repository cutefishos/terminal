// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include <algorithm>
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon.h>

#include "base/strings.h"

#include "terminal.h"

namespace term {

// Keys that type no text, by keysym.
static GhosttyKey functionKey(uint32_t keysym)
{
    switch (keysym) {
    case XKB_KEY_Return: return GHOSTTY_KEY_ENTER;
    case XKB_KEY_Tab:
    case XKB_KEY_ISO_Left_Tab: return GHOSTTY_KEY_TAB;
    case XKB_KEY_BackSpace: return GHOSTTY_KEY_BACKSPACE;
    case XKB_KEY_Escape: return GHOSTTY_KEY_ESCAPE;
    case XKB_KEY_Delete: return GHOSTTY_KEY_DELETE;
    case XKB_KEY_Insert: return GHOSTTY_KEY_INSERT;
    case XKB_KEY_Home: return GHOSTTY_KEY_HOME;
    case XKB_KEY_End: return GHOSTTY_KEY_END;
    case XKB_KEY_Page_Up: return GHOSTTY_KEY_PAGE_UP;
    case XKB_KEY_Page_Down: return GHOSTTY_KEY_PAGE_DOWN;
    case XKB_KEY_Left: return GHOSTTY_KEY_ARROW_LEFT;
    case XKB_KEY_Right: return GHOSTTY_KEY_ARROW_RIGHT;
    case XKB_KEY_Up: return GHOSTTY_KEY_ARROW_UP;
    case XKB_KEY_Down: return GHOSTTY_KEY_ARROW_DOWN;
    case XKB_KEY_Menu: return GHOSTTY_KEY_CONTEXT_MENU;
    case XKB_KEY_Print: return GHOSTTY_KEY_PRINT_SCREEN;
    case XKB_KEY_Scroll_Lock: return GHOSTTY_KEY_SCROLL_LOCK;
    case XKB_KEY_Pause: return GHOSTTY_KEY_PAUSE;
    case XKB_KEY_Caps_Lock: return GHOSTTY_KEY_CAPS_LOCK;
    case XKB_KEY_Num_Lock: return GHOSTTY_KEY_NUM_LOCK;
    case XKB_KEY_Shift_L: return GHOSTTY_KEY_SHIFT_LEFT;
    case XKB_KEY_Shift_R: return GHOSTTY_KEY_SHIFT_RIGHT;
    case XKB_KEY_Control_L: return GHOSTTY_KEY_CONTROL_LEFT;
    case XKB_KEY_Control_R: return GHOSTTY_KEY_CONTROL_RIGHT;
    case XKB_KEY_Alt_L: return GHOSTTY_KEY_ALT_LEFT;
    case XKB_KEY_Alt_R:
    case XKB_KEY_ISO_Level3_Shift: return GHOSTTY_KEY_ALT_RIGHT;
    case XKB_KEY_Super_L:
    case XKB_KEY_Meta_L: return GHOSTTY_KEY_META_LEFT;
    case XKB_KEY_Super_R:
    case XKB_KEY_Meta_R: return GHOSTTY_KEY_META_RIGHT;
    case XKB_KEY_KP_Enter: return GHOSTTY_KEY_NUMPAD_ENTER;
    case XKB_KEY_KP_Add: return GHOSTTY_KEY_NUMPAD_ADD;
    case XKB_KEY_KP_Subtract: return GHOSTTY_KEY_NUMPAD_SUBTRACT;
    case XKB_KEY_KP_Multiply: return GHOSTTY_KEY_NUMPAD_MULTIPLY;
    case XKB_KEY_KP_Divide: return GHOSTTY_KEY_NUMPAD_DIVIDE;
    case XKB_KEY_KP_Decimal: return GHOSTTY_KEY_NUMPAD_DECIMAL;
    case XKB_KEY_KP_Separator: return GHOSTTY_KEY_NUMPAD_SEPARATOR;
    case XKB_KEY_KP_Equal: return GHOSTTY_KEY_NUMPAD_EQUAL;
    case XKB_KEY_KP_Up: return GHOSTTY_KEY_NUMPAD_UP;
    case XKB_KEY_KP_Down: return GHOSTTY_KEY_NUMPAD_DOWN;
    case XKB_KEY_KP_Left: return GHOSTTY_KEY_NUMPAD_LEFT;
    case XKB_KEY_KP_Right: return GHOSTTY_KEY_NUMPAD_RIGHT;
    case XKB_KEY_KP_Begin: return GHOSTTY_KEY_NUMPAD_BEGIN;
    case XKB_KEY_KP_Home: return GHOSTTY_KEY_NUMPAD_HOME;
    case XKB_KEY_KP_End: return GHOSTTY_KEY_NUMPAD_END;
    case XKB_KEY_KP_Insert: return GHOSTTY_KEY_NUMPAD_INSERT;
    case XKB_KEY_KP_Delete: return GHOSTTY_KEY_NUMPAD_DELETE;
    case XKB_KEY_KP_Page_Up: return GHOSTTY_KEY_NUMPAD_PAGE_UP;
    case XKB_KEY_KP_Page_Down: return GHOSTTY_KEY_NUMPAD_PAGE_DOWN;
    default: break;
    }
    if (keysym >= XKB_KEY_F1 && keysym <= XKB_KEY_F25)
        return GhosttyKey(GHOSTTY_KEY_F1 + int(keysym - XKB_KEY_F1));
    if (keysym >= XKB_KEY_KP_0 && keysym <= XKB_KEY_KP_9)
        return GhosttyKey(GHOSTTY_KEY_NUMPAD_0 + int(keysym - XKB_KEY_KP_0));
    return GHOSTTY_KEY_UNIDENTIFIED;
}

// The keys of a US layout by the character they type, and the one they type
// without Shift: a key event without a key code still names its key.
struct TypedKey
{
    char32_t character;
    GhosttyKey key;
    char32_t unshifted;
};

static constexpr TypedKey kTypedKeys[] = {
    {' ', GHOSTTY_KEY_SPACE, ' '},         {'`', GHOSTTY_KEY_BACKQUOTE, '`'},    {'~', GHOSTTY_KEY_BACKQUOTE, '`'},
    {'-', GHOSTTY_KEY_MINUS, '-'},         {'_', GHOSTTY_KEY_MINUS, '-'},        {'=', GHOSTTY_KEY_EQUAL, '='},
    {'+', GHOSTTY_KEY_EQUAL, '='},         {'[', GHOSTTY_KEY_BRACKET_LEFT, '['}, {'{', GHOSTTY_KEY_BRACKET_LEFT, '['},
    {']', GHOSTTY_KEY_BRACKET_RIGHT, ']'}, {'}', GHOSTTY_KEY_BRACKET_RIGHT, ']'}, {'\\', GHOSTTY_KEY_BACKSLASH, '\\'},
    {'|', GHOSTTY_KEY_BACKSLASH, '\\'},    {';', GHOSTTY_KEY_SEMICOLON, ';'},    {':', GHOSTTY_KEY_SEMICOLON, ';'},
    {'\'', GHOSTTY_KEY_QUOTE, '\''},       {'"', GHOSTTY_KEY_QUOTE, '\''},       {',', GHOSTTY_KEY_COMMA, ','},
    {'<', GHOSTTY_KEY_COMMA, ','},         {'.', GHOSTTY_KEY_PERIOD, '.'},       {'>', GHOSTTY_KEY_PERIOD, '.'},
    {'/', GHOSTTY_KEY_SLASH, '/'},         {'?', GHOSTTY_KEY_SLASH, '/'},        {'!', GHOSTTY_KEY_DIGIT_1, '1'},
    {'@', GHOSTTY_KEY_DIGIT_2, '2'},       {'#', GHOSTTY_KEY_DIGIT_3, '3'},      {'$', GHOSTTY_KEY_DIGIT_4, '4'},
    {'%', GHOSTTY_KEY_DIGIT_5, '5'},       {'^', GHOSTTY_KEY_DIGIT_6, '6'},      {'&', GHOSTTY_KEY_DIGIT_7, '7'},
    {'*', GHOSTTY_KEY_DIGIT_8, '8'},       {'(', GHOSTTY_KEY_DIGIT_9, '9'},      {')', GHOSTTY_KEY_DIGIT_0, '0'},
};

static GhosttyKey typedKey(char32_t character, char32_t* unshifted)
{
    if (character >= 'a' && character <= 'z') {
        *unshifted = character;
        return GhosttyKey(GHOSTTY_KEY_A + int(character - 'a'));
    }
    if (character >= 'A' && character <= 'Z') {
        *unshifted = character - 'A' + 'a';
        return GhosttyKey(GHOSTTY_KEY_A + int(character - 'A'));
    }
    if (character >= '0' && character <= '9') {
        *unshifted = character;
        return GhosttyKey(GHOSTTY_KEY_DIGIT_0 + int(character - '0'));
    }
    for (const TypedKey& typed : kTypedKeys) {
        if (typed.character == character) {
            *unshifted = typed.unshifted;
            return typed.key;
        }
    }
    return GHOSTTY_KEY_UNIDENTIFIED;
}

// The writing keys by evdev code, for layouts whose characters say nothing
// about the key, such as Cyrillic.
static GhosttyKey physicalKey(uint32_t keycode)
{
    static constexpr char kLetters[] = "qwertyuiop\0\0\0\0asdfghjkl\0\0\0\0\0zxcvbnm";
    if (keycode >= KEY_Q && keycode <= KEY_M) {
        const char letter = kLetters[keycode - KEY_Q];
        if (letter)
            return GhosttyKey(GHOSTTY_KEY_A + (letter - 'a'));
    }
    if (keycode >= KEY_1 && keycode <= KEY_9)
        return GhosttyKey(GHOSTTY_KEY_DIGIT_1 + int(keycode - KEY_1));
    switch (keycode) {
    case KEY_0: return GHOSTTY_KEY_DIGIT_0;
    case KEY_MINUS: return GHOSTTY_KEY_MINUS;
    case KEY_EQUAL: return GHOSTTY_KEY_EQUAL;
    case KEY_LEFTBRACE: return GHOSTTY_KEY_BRACKET_LEFT;
    case KEY_RIGHTBRACE: return GHOSTTY_KEY_BRACKET_RIGHT;
    case KEY_SEMICOLON: return GHOSTTY_KEY_SEMICOLON;
    case KEY_APOSTROPHE: return GHOSTTY_KEY_QUOTE;
    case KEY_GRAVE: return GHOSTTY_KEY_BACKQUOTE;
    case KEY_BACKSLASH: return GHOSTTY_KEY_BACKSLASH;
    case KEY_COMMA: return GHOSTTY_KEY_COMMA;
    case KEY_DOT: return GHOSTTY_KEY_PERIOD;
    case KEY_SLASH: return GHOSTTY_KEY_SLASH;
    case KEY_SPACE: return GHOSTTY_KEY_SPACE;
    case KEY_102ND: return GHOSTTY_KEY_INTL_BACKSLASH;
    default: return GHOSTTY_KEY_UNIDENTIFIED;
    }
}

static GhosttyMods modsOf(uint32_t modifiers)
{
    GhosttyMods mods = 0;
    if (modifiers & ShiftKey)
        mods |= GHOSTTY_MODS_SHIFT;
    if (modifiers & ControlKey)
        mods |= GHOSTTY_MODS_CTRL;
    if (modifiers & AltKey)
        mods |= GHOSTTY_MODS_ALT;
    if (modifiers & SuperKey)
        mods |= GHOSTTY_MODS_SUPER;
    return mods;
}

static bool hasControlCharacters(std::string_view text)
{
    for (const char c : text) {
        if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f)
            return true;
    }
    return false;
}

bool Terminal::sendKey(const KeyInput& input)
{
    char32_t unshifted = 0;
    GhosttyKey key = functionKey(input.keysym);
    // The character the key types before Control or Alt change it.
    std::string text = hasControlCharacters(input.text) ? std::string() : input.text;
    if (key == GHOSTTY_KEY_UNIDENTIFIED) {
        const char32_t character = xkb_keysym_to_utf32(input.keysym);
        if (text.empty() && character >= 0x20 && character != 0x7f)
            cutefish::base::strings::appendUtf8(text, character);
        key = typedKey(character, &unshifted);
        if (key == GHOSTTY_KEY_UNIDENTIFIED) {
            key = physicalKey(input.keycode);
            unshifted = xkb_keysym_to_utf32(xkb_keysym_to_lower(input.keysym));
        }
    } else if (key == GHOSTTY_KEY_ENTER || key == GHOSTTY_KEY_TAB || key == GHOSTTY_KEY_BACKSPACE
               || key == GHOSTTY_KEY_ESCAPE || key == GHOSTTY_KEY_NUMPAD_ENTER) {
        text.clear();
    }
    if (key == GHOSTTY_KEY_UNIDENTIFIED && text.empty())
        return false;

    GhosttyMods mods = modsOf(input.modifiers);
    // Shift+Tab arrives as ISO_Left_Tab, Shift held.
    GhosttyMods consumed = 0;
    if ((mods & GHOSTTY_MODS_SHIFT) && !text.empty() && unshifted
        && cutefish::base::strings::utf8ToUtf32(text) != std::u32string(1, unshifted)) {
        consumed |= GHOSTTY_MODS_SHIFT;
    }

    ghostty_key_encoder_set_from_terminal(m_keyEncoder, m_terminal);
    ghostty_key_event_set_action(m_keyEvent, input.release ? GHOSTTY_KEY_ACTION_RELEASE
                                             : input.repeat ? GHOSTTY_KEY_ACTION_REPEAT
                                                            : GHOSTTY_KEY_ACTION_PRESS);
    ghostty_key_event_set_key(m_keyEvent, key);
    ghostty_key_event_set_mods(m_keyEvent, mods);
    ghostty_key_event_set_consumed_mods(m_keyEvent, consumed);
    ghostty_key_event_set_composing(m_keyEvent, false);
    ghostty_key_event_set_unshifted_codepoint(m_keyEvent, uint32_t(unshifted));
    ghostty_key_event_set_utf8(m_keyEvent, GhosttyString{reinterpret_cast<const uint8_t*>(text.data()), text.size()});

    uint8_t buffer[128];
    size_t written = 0;
    GhosttyResult result = ghostty_key_encoder_encode(m_keyEncoder, m_keyEvent, buffer, sizeof(buffer), &written);
    std::string large;
    if (result == GHOSTTY_OUT_OF_SPACE) {
        large.resize(written);
        result = ghostty_key_encoder_encode(m_keyEncoder, m_keyEvent, reinterpret_cast<uint8_t*>(large.data()),
                                            large.size(), &written);
    }
    // The event points at the text, which goes with this call.
    ghostty_key_event_set_utf8(m_keyEvent, GhosttyString{});
    if (result != GHOSTTY_SUCCESS || written == 0)
        return false;
    send(large.empty() ? std::string_view(reinterpret_cast<const char*>(buffer), written)
                       : std::string_view(large.data(), written));
    return true;
}

bool Terminal::sendMouse(const MouseInput& input, double surfaceWidth, double surfaceHeight)
{
    ghostty_mouse_encoder_set_from_terminal(m_mouseEncoder, m_terminal);
    GhosttyMouseEncoderSize size = GHOSTTY_INIT_SIZED(GhosttyMouseEncoderSize);
    size.screen_width = uint32_t(std::max(1.0, surfaceWidth));
    size.screen_height = uint32_t(std::max(1.0, surfaceHeight));
    size.cell_width = uint32_t(m_cellWidth);
    size.cell_height = uint32_t(m_cellHeight);
    ghostty_mouse_encoder_set(m_mouseEncoder, GHOSTTY_MOUSE_ENCODER_OPT_SIZE, &size);
    const bool pressed = input.anyButtonPressed;
    ghostty_mouse_encoder_set(m_mouseEncoder, GHOSTTY_MOUSE_ENCODER_OPT_ANY_BUTTON_PRESSED, &pressed);

    ghostty_mouse_event_set_action(m_mouseEvent, input.action == MouseInput::Press     ? GHOSTTY_MOUSE_ACTION_PRESS
                                                 : input.action == MouseInput::Release ? GHOSTTY_MOUSE_ACTION_RELEASE
                                                                                       : GHOSTTY_MOUSE_ACTION_MOTION);
    if (input.button > 0)
        ghostty_mouse_event_set_button(m_mouseEvent, GhosttyMouseButton(input.button));
    else
        ghostty_mouse_event_clear_button(m_mouseEvent);
    ghostty_mouse_event_set_mods(m_mouseEvent, modsOf(input.modifiers));
    ghostty_mouse_event_set_position(m_mouseEvent, GhosttyMousePosition{float(input.x), float(input.y)});

    uint8_t buffer[128];
    size_t written = 0;
    if (ghostty_mouse_encoder_encode(m_mouseEncoder, m_mouseEvent, buffer, sizeof(buffer), &written) != GHOSTTY_SUCCESS
        || written == 0) {
        return false;
    }
    send(std::string_view(reinterpret_cast<const char*>(buffer), written));
    return true;
}

}
