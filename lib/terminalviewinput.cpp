// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>

#include <xkbcommon/xkbcommon-keysyms.h>

#include "base/strings.h"
#include "ui/clipboard.h"
#include "ui/drag.h"
#include "ui/window.h"

#include "links.h"
#include "session.h"
#include "terminalview.h"

namespace ui = cutefish::ui;

namespace term {

// Lines a wheel notch scrolls.
static constexpr int kWheelLines = 3;
// How far a press on the selection moves before it drags the text out.
static constexpr double kDragDistance = 10;

static uint32_t buttonBit(ui::MouseButton button)
{
    return 1u << int(button);
}

static int programButton(ui::MouseButton button)
{
    switch (button) {
    case ui::MouseButton::Left: return 1;
    case ui::MouseButton::Right: return 2;
    case ui::MouseButton::Middle: return 3;
    case ui::MouseButton::Back: return 8;
    case ui::MouseButton::Forward: return 9;
    }
    return 0;
}

bool TerminalView::sendKey(const ui::KeyEvent& event)
{
    Terminal* terminal = this->terminal();
    if (!terminal)
        return false;
    KeyInput key;
    key.keysym = event.key;
    key.keycode = event.keycode;
    key.text = event.text;
    key.modifiers = event.modifiers;
    key.repeat = event.repeat;
    return terminal->sendKey(key);
}

// Shift with the arrows, the page keys, Home and End scrolls the history
// while no full-screen program has the screen; Control+Shift with Up and
// Down goes from prompt to prompt the shell marked, whatever has the screen.
bool TerminalView::handleScrollKey(const ui::KeyEvent& event)
{
    Terminal* terminal = this->terminal();
    if (terminal && event.modifiers == (ui::ControlModifier | ui::ShiftModifier)
        && (event.key == XKB_KEY_Up || event.key == XKB_KEY_Down)) {
        terminal->scrollToPrompt(event.key == XKB_KEY_Up ? -1 : 1);
        setScrollOffset(0);
        scrollChanged();
        return true;
    }
    if (!terminal || event.modifiers != ui::ShiftModifier || terminal->usesAlternateScreen())
        return false;
    switch (event.key) {
    case XKB_KEY_Up: scrollLines(-1); return true;
    case XKB_KEY_Down: scrollLines(1); return true;
    case XKB_KEY_Page_Up: scrollLines(-std::max(1, m_lines - 1)); return true;
    case XKB_KEY_Page_Down: scrollLines(std::max(1, m_lines - 1)); return true;
    case XKB_KEY_Home: setScrollbarValue(0); return true;
    case XKB_KEY_End: setScrollbarValue(scrollbarMaximum()); return true;
    default: return false;
    }
}

static bool isModifierKey(uint32_t key)
{
    return (key >= XKB_KEY_Shift_L && key <= XKB_KEY_Hyper_R) || key == XKB_KEY_ISO_Level3_Shift
            || key == XKB_KEY_ISO_Level5_Shift;
}

bool TerminalView::keyPress(const ui::KeyEvent& event)
{
    if ((event.key == XKB_KEY_Control_L || event.key == XKB_KEY_Control_R) && isHovered())
        updateLinkHover(m_lastPointer, m_lastModifiers | ui::ControlModifier);
    if (onShortcut && onShortcut(event))
        return true;
    if (handleScrollKey(event))
        return true;
    m_pressedKey = event;
    if (sendKey(event)) {
        // Typing lets the selection go, as anything but a modifier the
        // program hears does.
        if (!isModifierKey(event.key))
            clearSelection();
        terminal()->scrollToBottom();
        setScrollOffset(0);
        scrollChanged();
        restartCursorBlink();
    }
    return true;
}

bool TerminalView::keyRelease(const ui::KeyEvent& event)
{
    if (event.key == XKB_KEY_Control_L || event.key == XKB_KEY_Control_R)
        updateLinkHover(m_lastPointer, m_lastModifiers & ~ui::ControlModifier);
    if (m_pressedKey && m_pressedKey->key == event.key)
        m_pressedKey.reset();
    sendKeyRelease(event);
    return false;
}

// Only a program that asked for key releases hears them.
void TerminalView::sendKeyRelease(const ui::KeyEvent& event)
{
    if (Terminal* terminal = this->terminal()) {
        KeyInput key;
        key.keysym = event.key;
        key.keycode = event.keycode;
        key.text = event.text;
        key.modifiers = event.modifiers;
        key.release = true;
        terminal->sendKey(key);
    }
}

// Keys held as the keyboard leaves are released for the program, which will
// not hear it otherwise: the last key pressed, then each modifier it held,
// both sides.
void TerminalView::releaseHeldKey()
{
    if (!m_pressedKey)
        return;
    ui::KeyEvent event = *m_pressedKey;
    m_pressedKey.reset();
    sendKeyRelease(event);
    static constexpr struct {
        uint32_t modifier;
        uint32_t keys[2];
    } modifiers[] = {
        {ui::ShiftModifier, {XKB_KEY_Shift_L, XKB_KEY_Shift_R}},
        {ui::ControlModifier, {XKB_KEY_Control_L, XKB_KEY_Control_R}},
        {ui::AltModifier, {XKB_KEY_Alt_L, XKB_KEY_Alt_R}},
        {ui::SuperModifier, {XKB_KEY_Super_L, XKB_KEY_Super_R}},
    };
    const uint32_t original = event.key;
    for (const auto& [modifier, keys] : modifiers) {
        if (!(event.modifiers & modifier))
            continue;
        event.modifiers &= ~modifier;
        event.text.clear();
        event.keycode = 0;
        for (const uint32_t key : keys) {
            if (key == original)
                continue;
            event.key = key;
            sendKeyRelease(event);
        }
    }
}

ui::TextInputState TerminalView::textInputState() const
{
    ui::TextInputState state;
    const CellMetrics& cell = m_font.metrics();
    const double s = m_font.scale();
    const int column = std::max(0, m_lastCursor.first);
    const int line = std::max(0, m_lastCursor.second);
    const ui::PointF origin = mapToWindow({0, 0});
    state.cursorRectangle = {origin.x + (gridLeft() + column * cell.width) / s,
                             origin.y + (gridTop() + line * cell.height) / s - m_scrollOffset, cell.width / s,
                             cell.height / s};
    return state;
}

void TerminalView::textInput(const ui::TextInputEvent& event)
{
    if (!event.commit.empty() || !event.preedit.empty() || !m_preedit.empty())
        clearSelection();
    if (Terminal* terminal = this->terminal(); terminal && !event.commit.empty()) {
        terminal->send(event.commit);
        terminal->scrollToBottom();
        scrollChanged();
    }
    m_preedit = cutefish::base::strings::utf8ToUtf32(event.preedit);
    requestPaint();
}

bool TerminalView::programTakesMouse(uint32_t modifiers) const
{
    return terminal() && terminal()->tracksMouse() && !(modifiers & ui::ShiftModifier);
}

bool TerminalView::sendMouse(MouseInput::Action action, int button, ui::PointF position, uint32_t modifiers)
{
    Terminal* terminal = this->terminal();
    if (!terminal)
        return false;
    const CellMetrics& cell = m_font.metrics();
    const double s = m_font.scale();
    MouseInput mouse;
    mouse.action = action;
    mouse.button = button;
    mouse.modifiers = modifiers;
    mouse.x = std::max(0.0, position.x * s - gridLeft());
    mouse.y = std::max(0.0, position.y * s - gridTop());
    mouse.anyButtonPressed = m_buttons != 0;
    return terminal->sendMouse(mouse, double(m_columns * cell.width), double(m_lines * cell.height));
}

bool TerminalView::scroll(const ui::ScrollEvent& event)
{
    // A tab moved to another window brings its view; the scroller ticks on that window's frames.
    if (window() && (!m_scroller || m_scroller->window() != window()))
        m_scroller = std::make_unique<ui::Scroller>(window(), this);

    // Shift keeps clicks for selecting, but the wheel always goes to a
    // program that takes the mouse.
    const bool program = terminal() && terminal()->tracksMouse();
    // A full-screen program without the mouse that asked for alternate
    // scrolling, such as less, gets Up and Down instead.
    const bool arrows = !program && terminal() && terminal()->usesAlternateScreen()
            && terminal()->mode(GHOSTTY_MODE_MOUSE_ALTERNATE_SCROLL);
    // A touchpad drags the history by the pixel, coasts and stretches at the ends.
    if (!program && !arrows && event.precise && scrollbarMaximum() > 0 && m_scroller) {
        m_scroller->handleScroll(event);
        return true;
    }

    // Lines towards the top, from pixels for a touchpad and 120ths of a notch for a wheel.
    int lines = 0;
    if (event.precise) {
        m_wheelRemainder -= event.deltaY;
        const double line = cellHeightLogical();
        lines = int(m_wheelRemainder / line);
        m_wheelRemainder -= lines * line;
    } else {
        m_wheelRemainder -= event.value120Y * kWheelLines / 120.0;
        lines = int(m_wheelRemainder);
        m_wheelRemainder -= lines;
    }
    if (lines == 0)
        return true;

    if (program) {
        clearSelection();
        sendMouse(MouseInput::Press, lines > 0 ? 4 : 5, m_lastPointer, event.modifiers);
        return true;
    }
    if (arrows) {
        clearSelection();
        ui::KeyEvent key;
        key.key = lines > 0 ? XKB_KEY_Up : XKB_KEY_Down;
        for (int i = 0; i < std::abs(lines); ++i)
            sendKey(key);
    } else if (scrollbarMaximum() > 0) {
        setScrollbarValue(scrollbarValue() - lines);
    }
    return true;
}

GhosttyGridRef TerminalView::refAt(ui::PointF position, bool* ok) const
{
    const auto [column, line] = cellAt(position);
    return terminal()->viewportRef(column, line, ok);
}

static uint64_t monotonicNanoseconds()
{
    return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now().time_since_epoch())
                            .count());
}

void TerminalView::selectionPress(const ui::PointerEvent& event, bool apply)
{
    Terminal* terminal = this->terminal();
    bool ok = false;
    GhosttyGridRef ref = refAt(event.position, &ok);
    if (!ok)
        return;
    const double s = m_font.scale();
    const GhosttySurfacePosition position{event.position.x * s, event.position.y * s};
    const uint64_t time = monotonicNanoseconds();
    ghostty_selection_gesture_event_set(m_pressEvent, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REF, &ref);
    ghostty_selection_gesture_event_set(m_pressEvent, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_POSITION, &position);
    ghostty_selection_gesture_event_set(m_pressEvent, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_TIME_NS, &time);
    // The window counts the clicks; the gesture, whose interval and distance
    // are 0 unless set, takes a press as a repeat only when the window did.
    const bool repeat = event.clickCount > 1;
    const uint64_t interval = repeat ? UINT64_MAX : 0;
    const double distance = repeat ? HUGE_VAL : 0;
    ghostty_selection_gesture_event_set(m_pressEvent, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REPEAT_INTERVAL_NS, &interval);
    ghostty_selection_gesture_event_set(m_pressEvent, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REPEAT_DISTANCE, &distance);
    // With Control a triple click takes a command's whole output, as the shell marked it.
    const GhosttySelectionGestureBehaviors behaviors{
            GHOSTTY_SELECTION_GESTURE_BEHAVIOR_CELL, GHOSTTY_SELECTION_GESTURE_BEHAVIOR_WORD,
            (event.modifiers & ui::ControlModifier) ? GHOSTTY_SELECTION_GESTURE_BEHAVIOR_OUTPUT
                                                    : GHOSTTY_SELECTION_GESTURE_BEHAVIOR_LINE};
    ghostty_selection_gesture_event_set(m_pressEvent, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_BEHAVIORS, &behaviors);
    GhosttySelection selection{};
    GhosttyResult result = ghostty_selection_gesture_event(m_gesture, terminal->handle(), m_pressEvent, &selection);
    if (!apply)
        return;
    // A double click on a link takes the link whole, past the characters
    // that end a word.
    uint8_t clicks = 0;
    ghostty_selection_gesture_get(m_gesture, terminal->handle(), GHOSTTY_SELECTION_GESTURE_DATA_CLICK_COUNT, &clicks);
    if (clicks == 2 && linkSelection(event.position, &selection))
        result = GHOSTTY_SUCCESS;
    if (result == GHOSTTY_SUCCESS)
        terminal->setSelection(selection);
    else
        terminal->clearSelection();
    m_selecting = true;
    selectionChanged();
}

bool TerminalView::linkSelection(ui::PointF position, GhosttySelection* selection) const
{
    Terminal* terminal = this->terminal();
    const auto [column, line] = cellAt(position);
    const Terminal::LogicalLine text = terminal->logicalLine(line);
    for (size_t i = 0; i < text.cells.size(); ++i) {
        if (text.cells[i] != std::pair<int, int>{line, column})
            continue;
        const Link link = term::linkAt(text.text, i);
        if (!link.isValid())
            return false;
        const auto [firstLine, firstColumn] = text.cells[link.begin];
        const auto [lastLine, lastColumn] = text.cells[link.end - 1];
        bool startOk = false;
        bool endOk = false;
        selection->start = terminal->viewportRef(firstColumn, firstLine, &startOk);
        selection->end = terminal->viewportRef(lastColumn, lastLine, &endOk);
        selection->rectangle = false;
        return startOk && endOk;
    }
    return false;
}

void TerminalView::selectionDrag(ui::PointF point)
{
    Terminal* terminal = this->terminal();
    bool ok = false;
    GhosttyGridRef ref = refAt(point, &ok);
    if (!ok)
        return;
    const CellMetrics& cell = m_font.metrics();
    const double s = m_font.scale();
    const GhosttySurfacePosition position{point.x * s, point.y * s};
    const GhosttySelectionGestureGeometry geometry{uint32_t(m_columns), uint32_t(cell.width), uint32_t(gridLeft()),
                                                   uint32_t(std::max(1.0, this->geometry().height * s))};
    // Control and Alt together select a rectangle, as columns of output are.
    const bool rectangle = (m_lastModifiers & ui::ControlModifier) && (m_lastModifiers & ui::AltModifier);
    ghostty_selection_gesture_event_set(m_dragEvent, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REF, &ref);
    ghostty_selection_gesture_event_set(m_dragEvent, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_POSITION, &position);
    ghostty_selection_gesture_event_set(m_dragEvent, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_GEOMETRY, &geometry);
    ghostty_selection_gesture_event_set(m_dragEvent, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_RECTANGLE, &rectangle);
    GhosttySelection selection{};
    if (ghostty_selection_gesture_event(m_gesture, terminal->handle(), m_dragEvent, &selection) == GHOSTTY_SUCCESS)
        terminal->setSelection(selection);

    GhosttySelectionGestureAutoscroll direction = GHOSTTY_SELECTION_GESTURE_AUTOSCROLL_NONE;
    ghostty_selection_gesture_get(m_gesture, terminal->handle(), GHOSTTY_SELECTION_GESTURE_DATA_AUTOSCROLL, &direction);
    if (direction == GHOSTTY_SELECTION_GESTURE_AUTOSCROLL_NONE)
        m_autoscroll.stop();
    else if (!m_autoscroll.isActive())
        m_autoscroll.start();
    selectionChanged();
}

void TerminalView::autoscroll()
{
    Terminal* terminal = this->terminal();
    if (!terminal || !m_selecting) {
        m_autoscroll.stop();
        return;
    }
    const CellMetrics& cell = m_font.metrics();
    const double s = m_font.scale();
    const auto [column, line] = cellAt(m_lastPointer);
    const GhosttyPointCoordinate viewport{uint16_t(column), uint32_t(line)};
    const GhosttySurfacePosition position{m_lastPointer.x * s, m_lastPointer.y * s};
    const GhosttySelectionGestureGeometry geometry{uint32_t(m_columns), uint32_t(cell.width), uint32_t(gridLeft()),
                                                   uint32_t(std::max(1.0, this->geometry().height * s))};
    ghostty_selection_gesture_event_set(m_tickEvent, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_VIEWPORT, &viewport);
    ghostty_selection_gesture_event_set(m_tickEvent, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_POSITION, &position);
    ghostty_selection_gesture_event_set(m_tickEvent, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_GEOMETRY, &geometry);
    GhosttySelection selection{};
    if (ghostty_selection_gesture_event(m_gesture, terminal->handle(), m_tickEvent, &selection) != GHOSTTY_SUCCESS) {
        m_autoscroll.stop();
        return;
    }
    terminal->setSelection(selection);
    scrollChanged();
    selectionChanged();
}

void TerminalView::selectionRelease(ui::PointF point)
{
    Terminal* terminal = this->terminal();
    m_selecting = false;
    m_autoscroll.stop();
    bool ok = false;
    GhosttyGridRef ref = refAt(point, &ok);
    ghostty_selection_gesture_event_set(m_releaseEvent, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REF, ok ? &ref : nullptr);
    ghostty_selection_gesture_event(m_gesture, terminal->handle(), m_releaseEvent, nullptr);
    if (terminal->hasSelection())
        ui::Clipboard::setText(terminal->selectedText(), ui::ClipboardMode::Selection);
}

void TerminalView::startTextDrag()
{
    m_drag = Drag::Dragging;
    const std::string text = selectedText();
    ui::DragSource source;
    source.mimeTypes = {"text/plain;charset=utf-8", "text/plain"};
    source.data = [text](const std::string&) { return text; };
    source.actions = ui::CopyAction;
    std::weak_ptr<bool> alive = m_alive;
    source.onFinished = [this, alive](ui::DropAction) {
        if (alive.lock())
            m_drag = Drag::None;
    };
    startDrag(std::move(source));
}

void TerminalView::pointerPress(const ui::PointerEvent& event)
{
    m_buttons |= buttonBit(event.button);
    m_lastPointer = event.position;
    m_lastModifiers = event.modifiers;
    if (!terminal())
        return;

    m_menuPress = event.button == ui::MouseButton::Right
            && (!terminal()->tracksMouse() || (event.modifiers & ui::ShiftModifier));
    m_pressedLink = event.button == ui::MouseButton::Left && showsLinks(event.modifiers)
            ? linkAt(event.position.x, event.position.y)
            : std::string();
    if (!m_pressedLink.empty() || m_menuPress)
        return;

    if (programTakesMouse(event.modifiers)) {
        // What the program does with the click may move the text under the selection.
        clearSelection();
        m_mousePress = true;
        sendMouse(MouseInput::Press, programButton(event.button), event.position, event.modifiers);
        return;
    }
    if (event.button == ui::MouseButton::Left) {
        // Shift+click carries the selection's far end to the pointer, by
        // the unit the selection was made in.
        uint8_t clicks = 0;
        ghostty_selection_gesture_get(m_gesture, terminal()->handle(), GHOSTTY_SELECTION_GESTURE_DATA_CLICK_COUNT,
                                      &clicks);
        if ((event.modifiers & ui::ShiftModifier) && event.clickCount == 1 && clicks > 0 && hasSelection()) {
            m_selecting = true;
            selectionDrag(event.position);
            return;
        }
        if (event.clickCount == 1 && isSelectedAt(event.position.x, event.position.y)) {
            m_drag = Drag::Pending;
            m_dragStart = event.position;
            // Counted all the same, so that a second click selects a word there.
            selectionPress(event, false);
            return;
        }
        selectionPress(event);
    } else if (event.button == ui::MouseButton::Middle) {
        pasteSelection();
    }
}

void TerminalView::pointerMove(const ui::PointerEvent& event)
{
    const bool moved = event.position != m_lastPointer;
    m_lastPointer = event.position;
    m_lastModifiers = event.modifiers;
    if (!m_pressedLink.empty() || m_menuPress || !terminal())
        return;
    if (m_mousePress) {
        sendMouse(MouseInput::Motion, 0, event.position, event.modifiers);
        return;
    }
    if (m_drag == Drag::Pending) {
        if (std::abs(event.position.x - m_dragStart.x) > kDragDistance
            || std::abs(event.position.y - m_dragStart.y) > kDragDistance) {
            startTextDrag();
        }
        return;
    }
    // A move that goes nowhere, as one before a release, keeps what the
    // click selected, such as a whole link.
    if (moved && m_selecting && (m_buttons & buttonBit(ui::MouseButton::Left)))
        selectionDrag(event.position);
}

void TerminalView::pointerRelease(const ui::PointerEvent& event)
{
    m_buttons &= ~buttonBit(event.button);
    if (!m_pressedLink.empty()) {
        const std::string link = std::exchange(m_pressedLink, std::string());
        if (onLinkActivated)
            onLinkActivated(link);
        return;
    }
    if (m_menuPress) {
        m_menuPress = false;
        if (onContextMenu)
            onContextMenu(event.position.x, event.position.y);
        return;
    }
    if (!terminal())
        return;
    if (m_mousePress) {
        sendMouse(MouseInput::Release, programButton(event.button), event.position, event.modifiers);
        if (m_buttons == 0)
            m_mousePress = false;
        return;
    }
    if (event.button == ui::MouseButton::Left) {
        if (m_drag == Drag::Pending) {
            // A click on the selection without a drag lets it go.
            terminal()->clearSelection();
            selectionChanged();
        } else if (m_selecting) {
            selectionRelease(event.position);
        }
        m_drag = Drag::None;
        const ui::RectF bounds = localRect();
        if (event.position.x >= 0 && event.position.y >= 0 && event.position.x < bounds.width
            && event.position.y < bounds.height) {
            setFocus(ui::FocusReason::Mouse);
        }
    }
}

void TerminalView::hoverMove(const ui::PointerEvent& event)
{
    m_lastPointer = event.position;
    m_lastModifiers = event.modifiers;
    if (m_buttons != 0)
        return;
    updateLinkHover(event.position, event.modifiers);
    if (terminal() && terminal()->tracksMouse())
        sendMouse(MouseInput::Motion, 0, event.position, event.modifiers);
}

void TerminalView::hoverChanged()
{
    if (!isHovered())
        clearLinkHover();
}

void TerminalView::updatePointerShape()
{
    if (!m_linkSegments.empty())
        View::setCursorShape(ui::CursorShape::Pointer);
    else
        View::setCursorShape(terminal() && terminal()->tracksMouse() ? ui::CursorShape::Default : ui::CursorShape::Text);
}

// Links show, and open on a click, with Control alone held; a program
// taking the mouse leaves them to Shift as well.
bool TerminalView::showsLinks(uint32_t modifiers) const
{
    Terminal* terminal = this->terminal();
    if (!terminal || (terminal->tracksMouse() && !(modifiers & ui::ShiftModifier)))
        return false;
    return (modifiers & ~ui::ShiftModifier) == ui::ControlModifier;
}

// Only a move to another cell, or the modifiers, can change the link under the pointer.
void TerminalView::updateLinkHover(ui::PointF position, uint32_t modifiers)
{
    m_lastModifiers = modifiers;
    if (!showsLinks(modifiers)) {
        clearLinkHover();
        return;
    }
    Terminal* terminal = this->terminal();
    const std::pair<int, int> cell = cellAt(position);
    if (cell == m_hoverCell)
        return;
    m_hoverCell = cell;

    std::vector<Segment> segments;
    const auto [column, line] = cell;
    if (const std::string uri = terminal->hyperlinkAt(column, line); !uri.empty()) {
        int first = column;
        int last = column;
        while (first > 0 && terminal->hyperlinkAt(first - 1, line) == uri)
            --first;
        while (last + 1 < m_columns && terminal->hyperlinkAt(last + 1, line) == uri)
            ++last;
        segments.push_back({line, first, last - first + 1});
    } else {
        const Terminal::LogicalLine text = terminal->logicalLine(line);
        for (size_t i = 0; i < text.cells.size(); ++i) {
            if (text.cells[i] != std::pair<int, int>{line, column})
                continue;
            const Link link = term::linkAt(text.text, i);
            if (!link.isValid())
                break;
            // Each line the link covers, from its first cell to its last.
            std::map<int, std::pair<int, int>> lines;
            for (size_t byte = link.begin; byte < link.end; ++byte) {
                const auto [row, at] = text.cells[byte];
                auto [entry, added] = lines.try_emplace(row, at, at);
                entry->second.first = std::min(entry->second.first, at);
                entry->second.second = std::max(entry->second.second, at);
            }
            for (const auto& [row, range] : lines)
                segments.push_back({row, range.first, range.second - range.first + 1});
            break;
        }
    }
    const bool changed = segments.size() != m_linkSegments.size() || !segments.empty() || !m_linkSegments.empty();
    m_linkSegments = std::move(segments);
    updatePointerShape();
    if (changed)
        requestPaint();
}

void TerminalView::clearLinkHover()
{
    m_hoverCell = {-1, -1};
    if (!m_linkSegments.empty()) {
        m_linkSegments.clear();
        updatePointerShape();
        requestPaint();
    }
}

}
