// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <ghostty/vt.h>

namespace term {

struct ColorScheme;

// A key as the window system gives it: an xkb keysym, the evdev code of the
// physical key (0 when unknown), the text it types and the modifiers, with the
// toolkit's bits (Shift 1, Control 2, Alt 4, Super 8).
struct KeyInput
{
    uint32_t keysym = 0;
    uint32_t keycode = 0;
    std::string text;
    uint32_t modifiers = 0;
    bool repeat = false;
    bool release = false;
};

enum KeyModifier : uint32_t {
    ShiftKey = 1 << 0,
    ControlKey = 1 << 1,
    AltKey = 1 << 2,
    SuperKey = 1 << 3,
};

// A pointer event for a program that tracks the mouse, in pixels of the grid's
// surface. Buttons 4 to 7 are the wheel.
struct MouseInput
{
    enum Action { Press, Release, Motion };
    Action action = Press;
    // 0 for none, 1 left, 2 right, 3 middle, 4 and 5 wheel up and down.
    int button = 0;
    uint32_t modifiers = 0;
    double x = 0;
    double y = 0;
    bool anyButtonPressed = false;
};

// One terminal's state: the screens, the scrollback, the modes, the colours
// and what it answers, kept by libghostty-vt. Program output goes in through
// write(); what the terminal sends back, replies and encoded input alike,
// comes out through onSend.
class Terminal
{
public:
    Terminal(int columns = 80, int lines = 24);
    ~Terminal();

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;

    GhosttyTerminal handle() const { return m_terminal; }

    void write(std::string_view output);

    // The cell size in device pixels goes to size reports and mouse encoding.
    void resize(int columns, int lines, int cellWidth, int cellHeight);
    int columns() const { return m_columns; }
    int lines() const { return m_lines; }
    int cellWidth() const { return m_cellWidth; }
    int cellHeight() const { return m_cellHeight; }

    // Lines kept above the screen; negative keeps everything.
    void setScrollbackLines(int lines);
    void setColorScheme(const ColorScheme& scheme);
    bool darkBackground() const { return m_dark; }

    std::string title() const;
    // The shell marks its prompts (OSC 133).
    bool hasPromptMarks() const { return m_promptMarks; }
    // The cursor is on a prompt or its command line, as the shell marked them;
    // false without marks.
    bool cursorAtPrompt() const;
    // What a click on the command line asks of the terminal, as the shell
    // said at its prompt: nothing, arrow keys to the cell, or a mouse event
    // with the row counted from the screen's top or the prompt's.
    enum class PromptClick { None, Line, AbsoluteEvents, RelativeEvents };
    PromptClick promptClick() const { return m_promptClick; }
    // The directory the shell last reported (OSC 7), as a path; empty for none.
    std::string reportedDirectory() const;
    bool mode(GhosttyMode mode) const;
    bool usesAlternateScreen() const;
    bool bracketedPaste() const { return mode(GHOSTTY_MODE_BRACKETED_PASTE); }
    bool tracksMouse() const;
    int cursorColumn() const;
    int cursorLine() const;

    struct Scrollbar
    {
        uint64_t total = 0;
        uint64_t offset = 0;
        uint64_t length = 0;

        bool operator==(const Scrollbar&) const = default;
    };
    Scrollbar scrollbar() const;
    uint64_t scrollbackLines() const;
    void scrollToRow(uint64_t row);
    void scrollBy(int lines);
    // The viewport's top to a marked prompt that many before or after it.
    void scrollToPrompt(int prompts);
    void scrollToBottom();

    // Whether anything went to the program.
    bool sendKey(const KeyInput& key);
    bool sendMouse(const MouseInput& mouse, double surfaceWidth, double surfaceHeight);
    void sendFocus(bool focused);
    // Pasted as the modes ask: bracketed for a program that wants it, unsafe
    // control characters made spaces; a paste of text, not of a clipboard,
    // when `typed`. Unless `allowUnsafe`, text that could run commands is
    // refused and false returned.
    bool paste(std::string_view text, bool typed = false, bool allowUnsafe = true);
    // Bytes for the program as they are.
    void send(std::string_view bytes);

    // A full reset, scrollback and the colours programs set included.
    void reset();
    void clearScrollback();
    // Clears the scrollback and the lines above the cursor's, which stays where it is.
    void clearAboveCursor();
    // Clears the screen and the scrollback, keeping the cursor's line at the top.
    void clearKeepingCursorLine();
    void clearScreen();

    bool hasSelection() const;
    std::string selectedText() const;
    // The text of a selection snapshot, unwrapped and trimmed as a copy is.
    std::string text(const GhosttySelection& selection) const;
    void setSelection(const GhosttySelection& selection);
    void clearSelection();
    void selectAll();
    // In the viewport's cells.
    bool selectWord(int column, int line);
    bool isSelected(int column, int line) const;
    // The text of a line of the viewport with the lines it wraps into or from,
    // and where the given line starts in it; columns map to UTF-8 offsets.
    struct LogicalLine
    {
        std::string text;
        // For each byte, the viewport cell it came from (line, column).
        std::vector<std::pair<int, int>> cells;
    };
    LogicalLine logicalLine(int line) const;
    // An OSC 8 link on the cell, or empty.
    std::string hyperlinkAt(int column, int line) const;
    // A snapshot of the viewport cell, clamped into the viewport.
    GhosttyGridRef viewportRef(int column, int line, bool* ok) const;

    std::function<void(std::string_view bytes)> onSend;
    // After a write, once for any number of changes in it.
    std::function<void()> onTitleChanged;
    std::function<void()> onBell;
    // Synchronized output began or ended; called inside write().
    std::function<void(bool held)> onRenderHold;

private:
    friend struct TerminalCallbacks;

    void notePromptMarks(std::string_view output);
    void notePromptMark(std::string_view mark);

    GhosttyTerminal m_terminal = nullptr;
    GhosttyKeyEncoder m_keyEncoder = nullptr;
    GhosttyKeyEvent m_keyEvent = nullptr;
    GhosttyMouseEncoder m_mouseEncoder = nullptr;
    GhosttyMouseEvent m_mouseEvent = nullptr;
    int m_columns;
    int m_lines;
    int m_cellWidth = 8;
    int m_cellHeight = 16;
    bool m_dark = true;
    bool m_titleChanged = false;
    bool m_bell = false;
    std::string m_xtversion;
    bool m_promptMarks = false;
    PromptClick m_promptClick = PromptClick::None;
    std::string m_pendingMark;
};

}
