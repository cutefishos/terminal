// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include "terminalview.h"

#include <algorithm>
#include <cmath>

#include "ui/clipboard.h"
#include "ui/window.h"

#include "colorschemes.h"
#include "links.h"
#include "search.h"
#include "session.h"

namespace ui = cutefish::ui;

namespace term {

static constexpr auto kCursorBlinkInterval = std::chrono::milliseconds(500);
static constexpr auto kTextBlinkInterval = std::chrono::milliseconds(500);
// A program that holds the screen longer than this is drawn anyway.
static constexpr auto kRenderHoldLimit = std::chrono::seconds(1);
// The grid's inset from the view's edges, logical pixels.
static constexpr double kGridMargin = 1;

static ui::Color colorOf(uint32_t rgb)
{
    return ui::Color::rgb(rgb);
}

TerminalView::TerminalView()
{
    setName("terminal:view");
    setTracksHover(true);
    View::setCursorShape(ui::CursorShape::Text);

    ghostty_render_state_new(nullptr, &m_state);
    ghostty_render_state_row_iterator_new(nullptr, &m_rows);
    ghostty_render_state_row_cells_new(nullptr, &m_cells);
    // The row below the viewport, which a touchpad scroll between lines
    // shows part of; the grid only ever moves up from the viewport's top.
    const GhosttyRenderStateOverscan overscan{0, 1};
    ghostty_render_state_set(m_state, GHOSTTY_RENDER_STATE_OPT_OVERSCAN, &overscan);

    ghostty_selection_gesture_new(nullptr, &m_gesture);
    ghostty_selection_gesture_event_new(nullptr, &m_pressEvent, GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_PRESS);
    ghostty_selection_gesture_event_new(nullptr, &m_dragEvent, GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_DRAG);
    ghostty_selection_gesture_event_new(nullptr, &m_releaseEvent, GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_RELEASE);
    ghostty_selection_gesture_event_new(nullptr, &m_tickEvent, GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_AUTOSCROLL_TICK);

    m_cursorBlinkTimer.setInterval(kCursorBlinkInterval);
    m_cursorBlinkTimer.onTimeout = [this] { blinkCursor(); };
    m_textBlinkTimer.setInterval(kTextBlinkInterval);
    m_textBlinkTimer.onTimeout = [this] {
        m_textHidden = !m_textHidden;
        requestPaint();
    };
    m_holdTimeout.setSingleShot(true);
    m_holdTimeout.onTimeout = [this] {
        if (Terminal* terminal = this->terminal()) {
            GhosttyTerminalModeConfig mode{GHOSTTY_MODE_SYNCHRONIZED_OUTPUT, false};
            ghostty_terminal_set(terminal->handle(), GHOSTTY_TERMINAL_OPT_MODE, &mode);
        }
        m_held = false;
        scheduleUpdate();
    };
    m_autoscroll.setInterval(std::chrono::milliseconds(15));
    m_autoscroll.onTimeout = [this] { autoscroll(); };

    setColorScheme(std::string());
}

TerminalView::~TerminalView()
{
    *m_alive = false;
    m_search.reset();
    ghostty_selection_gesture_event_free(m_tickEvent);
    ghostty_selection_gesture_event_free(m_releaseEvent);
    ghostty_selection_gesture_event_free(m_dragEvent);
    ghostty_selection_gesture_event_free(m_pressEvent);
    ghostty_selection_gesture_free(m_gesture, terminal() ? terminal()->handle() : nullptr);
    if (Terminal* terminal = this->terminal())
        terminal->onRenderHold = nullptr;
    ghostty_render_state_row_cells_free(m_cells);
    ghostty_render_state_row_iterator_free(m_rows);
    ghostty_render_state_free(m_state);
}

Terminal* TerminalView::terminal() const
{
    return m_session ? &m_session->terminal() : nullptr;
}

void TerminalView::setSession(Session* session)
{
    m_sessionConnections.clear();
    m_session = session;
    if (!session)
        return;

    m_sessionConnections.push_back(session->outputReceived.connect([this] { scheduleUpdate(); }));
    m_sessionConnections.push_back(session->bell.connect([this] {
        if (m_notifiesBell)
            notifyBell();
    }));
    session->terminal().onRenderHold = [this](bool held) {
        // The frame the program wants left on screen is the terminal as it is now.
        if (held && !m_held) {
            ghostty_render_state_update(m_state, terminal()->handle());
            m_holdTimeout.start(kRenderHoldLimit);
        } else if (!held) {
            m_holdTimeout.stop();
        }
        m_held = held;
    };
    setColorScheme(m_colorScheme);
    setCursorShape(m_cursorShape);
    updateGeometry();
    scheduleUpdate();
}

void TerminalView::setColorScheme(const std::string& name)
{
    m_colorScheme = name;
    const ColorScheme& scheme = colorScheme(name);
    m_background = colorOf(scheme.background);
    m_foreground = colorOf(scheme.foreground);
    if (Terminal* terminal = this->terminal())
        terminal->setColorScheme(scheme);
    scheduleUpdate();
    requestPaint();
}

void TerminalView::setBackgroundOpacity(double opacity)
{
    m_opacity = std::clamp(opacity, 0.0, 1.0);
    requestPaint();
}

void TerminalView::setLinkColor(ui::Color color)
{
    m_linkColor = color;
    if (!m_linkSegments.empty())
        requestPaint();
}

void TerminalView::setTerminalFont(const std::string& family, double pointSize)
{
    m_fontFamily = family;
    m_fontPointSize = pointSize;
    m_font.setFont(family, pointSize);
    updateGeometry();
    requestPaint();
}

void TerminalView::setBlinkingCursor(bool blinking)
{
    m_blinkingCursor = blinking;
    if (!blinking) {
        m_cursorBlinkTimer.stop();
        m_cursorHidden = false;
    } else if (hasKeyboard() && !m_cursorBlinkTimer.isActive()) {
        m_cursorBlinkTimer.start();
    }
    if (m_lastCursor.second >= 0)
        requestPaint(rowRect(m_lastCursor.second));
}

void TerminalView::setCursorShape(int shape)
{
    m_cursorShape = std::clamp(shape, 0, 2);
    Terminal* terminal = this->terminal();
    if (terminal && m_cursorShape != m_appliedCursorShape) {
        m_appliedCursorShape = m_cursorShape;
        const GhosttyCursorStyle style = m_cursorShape == 1 ? GHOSTTY_CURSOR_STYLE_UNDERLINE
                : m_cursorShape == 2                       ? GHOSTTY_CURSOR_STYLE_BAR
                                                           : GHOSTTY_CURSOR_STYLE_BLOCK;
        ghostty_terminal_set(terminal->handle(), GHOSTTY_TERMINAL_OPT_DEFAULT_CURSOR_STYLE, &style);
        // DECSCUSR 0 takes the default, until a program sets a shape of its own.
        if (!terminal->usesAlternateScreen())
            terminal->write("\033[0 q");
        scheduleUpdate();
    }
}

double TerminalView::scale() const
{
    const ui::Window* window = this->window();
    return window ? window->scale() : 1;
}

double TerminalView::cellHeightLogical() const
{
    return m_font.metrics().height / m_font.scale();
}

int TerminalView::gridLeft() const
{
    return int(std::lround(kGridMargin * m_font.scale()));
}

int TerminalView::gridTop() const
{
    return int(std::lround(kGridMargin * m_font.scale()));
}

std::pair<int, int> TerminalView::cellAt(ui::PointF point) const
{
    const CellMetrics& cell = m_font.metrics();
    const double s = m_font.scale();
    const double x = point.x * s - gridLeft();
    const double y = point.y * s - gridTop() + m_scrollOffset * s;
    return {std::clamp(int(std::floor(x / cell.width)), 0, m_columns - 1),
            std::clamp(int(std::floor(y / cell.height)), 0, m_lines - 1)};
}

ui::RectF TerminalView::rowRect(int line, int count) const
{
    const double s = m_font.scale();
    const double top = (gridTop() + line * m_font.metrics().height) / s - m_scrollOffset;
    return {0, top - 1, geometry().width, count * cellHeightLogical() + 2};
}

void TerminalView::layout()
{
    if (window() && (!m_scroller || m_scroller->window() != window()))
        m_scroller = std::make_unique<ui::Scroller>(window(), this);
    updateGeometry();
    requestPaint();
}

void TerminalView::updateGeometry(double scale)
{
    m_font.setScale(scale > 0 ? scale : this->scale());
    const CellMetrics& cell = m_font.metrics();
    const double s = m_font.scale();
    const int width = int(std::floor(geometry().width * s)) - 2 * gridLeft();
    const int height = int(std::floor(geometry().height * s)) - 2 * gridTop();
    const int columns = std::max(1, width / cell.width);
    const int lines = std::max(1, height / cell.height);
    // A view not given its size yet, or folded away while its tab moves,
    // would reflow the whole history into one column.
    if (columns < 2 || lines < 2) {
        scheduleUpdate();
        return;
    }
    if (m_session)
        m_session->resize(columns, lines, cell.width, cell.height);
    if (columns != m_columns || lines != m_lines) {
        m_columns = columns;
        m_lines = lines;
        if (m_scroller)
            m_scroller->cancel(false);
        m_scrollOffset = 0;
        sizeChanged();
    }
    scheduleUpdate();
}

int TerminalView::scrollbarMaximum() const
{
    return int(m_scrollbar.total > m_scrollbar.length ? m_scrollbar.total - m_scrollbar.length : 0);
}

void TerminalView::setScrollbarValue(int value)
{
    if (Terminal* terminal = this->terminal()) {
        terminal->scrollToRow(uint64_t(std::clamp(value, 0, scrollbarMaximum())));
        setScrollOffset(0);
        scrollChanged();
    }
}

void TerminalView::scrollLines(int lines)
{
    if (Terminal* terminal = this->terminal()) {
        terminal->scrollBy(lines);
        setScrollOffset(0);
        scrollChanged();
    }
}

void TerminalView::setScrollOffset(double offset)
{
    if (offset == m_scrollOffset)
        return;
    m_scrollOffset = offset;
    requestPaint();
}

void TerminalView::scrollChanged()
{
    Terminal* terminal = this->terminal();
    if (!terminal)
        return;
    const Terminal::Scrollbar bar = terminal->scrollbar();
    if (bar == m_scrollbar)
        return;
    const bool moved = bar.offset != m_scrollbar.offset;
    m_scrollbar = bar;
    if (m_scroller && !m_applyingScroll)
        m_scroller->viewMoved();
    scrollbarParamsChanged();
    if (moved)
        scrollbarValueChanged();
    scheduleUpdate();
}

ui::Scroller::Axis TerminalView::horizontalAxis() const
{
    return {};
}

ui::Scroller::Axis TerminalView::verticalAxis() const
{
    const double maximum = scrollbarMaximum() * cellHeightLogical();
    return {0, maximum, geometry().height, maximum > 0};
}

ui::PointF TerminalView::position() const
{
    return {0, m_scrollbar.offset * cellHeightLogical() + m_scrollOffset};
}

void TerminalView::applyPosition(ui::PointF position)
{
    Terminal* terminal = this->terminal();
    if (!terminal)
        return;
    const double line = cellHeightLogical();
    const int row = std::clamp(int(std::floor(position.y / line)), 0, scrollbarMaximum());
    m_applyingScroll = true;
    if (uint64_t(row) != m_scrollbar.offset) {
        terminal->scrollToRow(uint64_t(row));
        scrollChanged();
    }
    m_applyingScroll = false;
    setScrollOffset(position.y - row * line);
}

bool TerminalView::hasKeyboard() const
{
    const ui::Window* window = this->window();
    return window && hasFocus() && window->hasKeyboardFocus();
}

void TerminalView::restartCursorBlink()
{
    m_cursorHidden = false;
    if (m_blinkingCursor && hasKeyboard())
        m_cursorBlinkTimer.start();
    if (m_lastCursor.second >= 0)
        requestPaint(rowRect(m_lastCursor.second));
}

void TerminalView::blinkCursor()
{
    // Without the keyboard the cursor stays drawn, in its unfocused state.
    if (!hasKeyboard()) {
        m_cursorBlinkTimer.stop();
        m_cursorHidden = false;
    } else {
        m_cursorHidden = !m_cursorHidden;
    }
    if (m_lastCursor.second >= 0)
        requestPaint(rowRect(m_lastCursor.second));
}

// A program that asked hears when the keyboard comes or goes, the view's
// focus or the window's.
void TerminalView::reportFocus()
{
    const bool focused = hasKeyboard();
    if (focused == m_reportedFocus)
        return;
    m_reportedFocus = focused;
    if (Terminal* terminal = this->terminal())
        terminal->sendFocus(focused);
}

void TerminalView::focusIn(ui::FocusReason)
{
    reportFocus();
    restartCursorBlink();
    requestPaint();
}

void TerminalView::focusOut()
{
    reportFocus();
    m_cursorBlinkTimer.stop();
    m_cursorHidden = false;
    requestPaint();
}

bool TerminalView::hasSelection() const
{
    return terminal() && terminal()->hasSelection();
}

std::string TerminalView::selectedText() const
{
    return terminal() ? terminal()->selectedText() : std::string();
}

void TerminalView::selectionChanged()
{
    scheduleUpdate();
    requestPaint();
}

void TerminalView::clearSelection()
{
    Terminal* terminal = this->terminal();
    if (!terminal || !terminal->hasSelection())
        return;
    terminal->clearSelection();
    selectionChanged();
}

void TerminalView::selectAll()
{
    if (Terminal* terminal = this->terminal()) {
        terminal->selectAll();
        ui::Clipboard::setText(terminal->selectedText(), ui::ClipboardMode::Selection);
        selectionChanged();
    }
}

void TerminalView::copyClipboard()
{
    const std::string text = selectedText();
    if (!text.empty())
        ui::Clipboard::setText(text);
}

void TerminalView::pasteClipboard()
{
    std::weak_ptr<bool> alive = m_alive;
    ui::Clipboard::requestText([this, alive](std::string text) {
        if (alive.lock())
            sendPaste(text, false);
    });
}

void TerminalView::pasteSelection()
{
    std::weak_ptr<bool> alive = m_alive;
    ui::Clipboard::requestText(
            [this, alive](std::string text) {
                if (alive.lock())
                    sendPaste(text, false);
            },
            ui::ClipboardMode::Selection);
}

void TerminalView::pasteText(const std::string& text)
{
    sendPaste(text, true);
}

void TerminalView::sendPaste(const std::string& text, bool typed)
{
    Terminal* terminal = this->terminal();
    if (!terminal || text.empty())
        return;
    // The library refuses what could run commands: line breaks outside a
    // bracketed paste, or the end of the brackets inside one.
    if (!terminal->paste(text, typed, !m_confirmMultilinePaste)) {
        m_pendingPaste = text;
        multilinePasteRequested(text);
        return;
    }
    terminal->scrollToBottom();
    scrollChanged();
}

void TerminalView::confirmPaste()
{
    Terminal* terminal = this->terminal();
    const std::string text = std::exchange(m_pendingPaste, std::string());
    if (!terminal || text.empty())
        return;
    terminal->scrollToBottom();
    terminal->paste(text);
    scrollChanged();
}

bool TerminalView::isSelectedAt(double x, double y) const
{
    const auto [column, line] = cellAt({x, y});
    return terminal() && terminal()->isSelected(column, line);
}

bool TerminalView::selectWordAt(double x, double y)
{
    const auto [column, line] = cellAt({x, y});
    Terminal* terminal = this->terminal();
    if (!terminal || !terminal->selectWord(column, line))
        return false;
    ui::Clipboard::setText(terminal->selectedText(), ui::ClipboardMode::Selection);
    selectionChanged();
    return true;
}

std::string TerminalView::linkAt(double x, double y) const
{
    Terminal* terminal = this->terminal();
    if (!terminal)
        return {};
    const auto [column, line] = cellAt({x, y});
    if (std::string uri = terminal->hyperlinkAt(column, line); !uri.empty())
        return uri;
    const Terminal::LogicalLine text = terminal->logicalLine(line);
    for (size_t i = 0; i < text.cells.size(); ++i) {
        if (text.cells[i] == std::pair<int, int>{line, column})
            return term::linkAt(text.text, i).url;
    }
    return {};
}

void TerminalView::find(const std::string& text, bool forwards, bool fromSelection, bool caseSensitive)
{
    Terminal* terminal = this->terminal();
    if (!terminal)
        return;
    // Typing searches again from the newest output.
    if (!m_search || !fromSelection)
        m_search = std::make_unique<Search>(*terminal);
    const bool found = m_search->find(text, forwards, caseSensitive);
    scrollChanged();
    selectionChanged();
    findResult(found || text.empty());
}

void TerminalView::clearFind()
{
    m_search.reset();
    if (Terminal* terminal = this->terminal())
        terminal->clearSelection();
    selectionChanged();
}

}
