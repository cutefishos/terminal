// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include "terminal.h"

#include <algorithm>
#include <cstdint>

#include "base/strings.h"

#include "colorschemes.h"

namespace term {

// What a program reads back from XTVERSION.
static constexpr std::string_view kVersion = "cutefish-terminal 1.0";

// The library calls these during a write, so they only note what happened or
// hand bytes on; nothing here writes to the terminal again.
struct TerminalCallbacks
{
    static Terminal& of(void* userdata) { return *static_cast<Terminal*>(userdata); }

    static void send(GhosttyTerminal, void* userdata, const uint8_t* data, size_t length)
    {
        Terminal& terminal = of(userdata);
        if (terminal.onSend)
            terminal.onSend(std::string_view(reinterpret_cast<const char*>(data), length));
    }

    static void bell(GhosttyTerminal, void* userdata) { of(userdata).m_bell = true; }
    static void title(GhosttyTerminal, void* userdata) { of(userdata).m_titleChanged = true; }

    static GhosttyString version(GhosttyTerminal, void* userdata)
    {
        const std::string& text = of(userdata).m_xtversion;
        return GhosttyString{reinterpret_cast<const uint8_t*>(text.data()), text.size()};
    }

    static bool size(GhosttyTerminal, void* userdata, GhosttySizeReportSize* size)
    {
        const Terminal& terminal = of(userdata);
        size->rows = uint16_t(terminal.m_lines);
        size->columns = uint16_t(terminal.m_columns);
        size->cell_width = uint32_t(terminal.m_cellWidth);
        size->cell_height = uint32_t(terminal.m_cellHeight);
        return true;
    }

    static bool colorScheme(GhosttyTerminal, void* userdata, GhosttyColorScheme* scheme)
    {
        *scheme = of(userdata).m_dark ? GHOSTTY_COLOR_SCHEME_DARK : GHOSTTY_COLOR_SCHEME_LIGHT;
        return true;
    }

    static void renderHold(GhosttyTerminal, void* userdata, bool held)
    {
        Terminal& terminal = of(userdata);
        if (terminal.onRenderHold)
            terminal.onRenderHold(held);
    }
};

Terminal::Terminal(int columns, int lines)
    : m_columns(std::max(1, columns))
    , m_lines(std::max(1, lines))
    , m_xtversion(kVersion)
{
    ghostty_terminal_new(nullptr, &m_terminal, uint16_t(m_columns), uint16_t(m_lines));
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_USERDATA, this);
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_WRITE_PTY,
                         reinterpret_cast<const void*>(&TerminalCallbacks::send));
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_BELL, reinterpret_cast<const void*>(&TerminalCallbacks::bell));
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_TITLE_CHANGED,
                         reinterpret_cast<const void*>(&TerminalCallbacks::title));
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_XTVERSION,
                         reinterpret_cast<const void*>(&TerminalCallbacks::version));
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_SIZE, reinterpret_cast<const void*>(&TerminalCallbacks::size));
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_COLOR_SCHEME,
                         reinterpret_cast<const void*>(&TerminalCallbacks::colorScheme));
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_RENDER_HOLD,
                         reinterpret_cast<const void*>(&TerminalCallbacks::renderHold));
    const GhosttyString terminfo{reinterpret_cast<const uint8_t*>("xterm-256color"), 14};
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_TERMINFO_NAME, &terminfo);
    setScrollbackLines(10000);
    // The view draws no images, so programs are not told that they show.
    const uint64_t noImages = 0;
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_KITTY_IMAGE_STORAGE_LIMIT, &noImages);

    ghostty_key_encoder_new(nullptr, &m_keyEncoder);
    ghostty_key_event_new(nullptr, &m_keyEvent);
    ghostty_mouse_encoder_new(nullptr, &m_mouseEncoder);
    ghostty_mouse_event_new(nullptr, &m_mouseEvent);
}

Terminal::~Terminal()
{
    ghostty_mouse_event_free(m_mouseEvent);
    ghostty_mouse_encoder_free(m_mouseEncoder);
    ghostty_key_event_free(m_keyEvent);
    ghostty_key_encoder_free(m_keyEncoder);
    ghostty_terminal_free(m_terminal);
}

void Terminal::write(std::string_view output)
{
    ghostty_terminal_vt_write(m_terminal, reinterpret_cast<const uint8_t*>(output.data()), output.size());
    if (m_titleChanged) {
        m_titleChanged = false;
        if (onTitleChanged)
            onTitleChanged();
    }
    if (m_bell) {
        m_bell = false;
        if (onBell)
            onBell();
    }
}

void Terminal::resize(int columns, int lines, int cellWidth, int cellHeight)
{
    m_columns = std::max(1, columns);
    m_lines = std::max(1, lines);
    m_cellWidth = std::max(1, cellWidth);
    m_cellHeight = std::max(1, cellHeight);
    ghostty_terminal_resize(m_terminal, uint16_t(m_columns), uint16_t(m_lines), uint32_t(m_cellWidth),
                            uint32_t(m_cellHeight));
}

void Terminal::setScrollbackLines(int lines)
{
    // The byte limit would cut history long before the line limit, so only lines count.
    const size_t unlimited = SIZE_MAX;
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_SCROLLBACK_MAX_BYTES, &unlimited);
    const size_t count = lines < 0 ? SIZE_MAX : size_t(lines);
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_SCROLLBACK_MAX_LINES, &count);
}

static GhosttyColorRgb rgb(uint32_t value)
{
    return GhosttyColorRgb{uint8_t(value >> 16), uint8_t(value >> 8), uint8_t(value)};
}

void Terminal::setColorScheme(const ColorScheme& scheme)
{
    const GhosttyColorRgb foreground = rgb(scheme.foreground);
    const GhosttyColorRgb background = rgb(scheme.background);
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_COLOR_FOREGROUND, &foreground);
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_COLOR_BACKGROUND, &background);

    GhosttyColorRgb palette[256];
    ghostty_color_palette_default(palette);
    for (size_t i = 0; i < scheme.colors.size(); ++i)
        palette[i] = rgb(scheme.colors[i]);
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_COLOR_PALETTE, palette);

    m_dark = 0.2126 * background.r + 0.7152 * background.g + 0.0722 * background.b < 127.5;
}

std::string Terminal::title() const
{
    GhosttyString title{};
    if (ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_TITLE, &title) != GHOSTTY_SUCCESS || !title.ptr)
        return {};
    return cutefish::base::strings::sanitizedUtf8(
            std::string_view(reinterpret_cast<const char*>(title.ptr), title.len));
}

bool Terminal::mode(GhosttyMode mode) const
{
    GhosttyTerminalModeConfig config{mode, false};
    ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_MODE, &config);
    return config.value;
}

bool Terminal::usesAlternateScreen() const
{
    GhosttyTerminalScreen screen = GHOSTTY_TERMINAL_SCREEN_PRIMARY;
    ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_ACTIVE_SCREEN, &screen);
    return screen == GHOSTTY_TERMINAL_SCREEN_ALTERNATE;
}

bool Terminal::tracksMouse() const
{
    bool tracking = false;
    ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_MOUSE_TRACKING, &tracking);
    return tracking;
}

int Terminal::cursorColumn() const
{
    uint16_t x = 0;
    ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_CURSOR_X, &x);
    return x;
}

int Terminal::cursorLine() const
{
    uint16_t y = 0;
    ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_CURSOR_Y, &y);
    return y;
}

Terminal::Scrollbar Terminal::scrollbar() const
{
    GhosttyTerminalScrollbar bar{};
    ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_SCROLLBAR, &bar);
    return {bar.total, bar.offset, bar.len};
}

uint64_t Terminal::scrollbackLines() const
{
    size_t rows = 0;
    ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_SCROLLBACK_ROWS, &rows);
    return rows;
}

void Terminal::scrollToRow(uint64_t row)
{
    GhosttyTerminalScrollViewport scroll{};
    scroll.tag = GHOSTTY_TERMINAL_SCROLL_VIEWPORT_ROW;
    scroll.value.row = size_t(row);
    ghostty_terminal_scroll_viewport(m_terminal, scroll);
}

void Terminal::scrollBy(int lines)
{
    GhosttyTerminalScrollViewport scroll{};
    scroll.tag = GHOSTTY_TERMINAL_SCROLL_VIEWPORT_DELTA;
    scroll.value.delta = lines;
    ghostty_terminal_scroll_viewport(m_terminal, scroll);
}

void Terminal::scrollToBottom()
{
    GhosttyTerminalScrollViewport scroll{};
    scroll.tag = GHOSTTY_TERMINAL_SCROLL_VIEWPORT_BOTTOM;
    ghostty_terminal_scroll_viewport(m_terminal, scroll);
}

void Terminal::send(std::string_view bytes)
{
    if (!bytes.empty() && onSend)
        onSend(bytes);
}

void Terminal::sendFocus(bool focused)
{
    if (!mode(GHOSTTY_MODE_FOCUS_EVENT))
        return;
    send(focused ? "\033[I" : "\033[O");
}

struct PasteSource
{
    std::string_view text;
};

static bool readPaste(void* userdata, GhosttyString, GhosttyWriter writer)
{
    const PasteSource& source = *static_cast<const PasteSource*>(userdata);
    return writer.write(writer.userdata, reinterpret_cast<const uint8_t*>(source.text.data()), source.text.size());
}

void Terminal::paste(std::string_view text, bool typed)
{
    if (text.empty())
        return;
    PasteSource source{text};
    const GhosttyString mime{reinterpret_cast<const uint8_t*>("text/plain"), 10};
    GhosttyPaste paste = GHOSTTY_INIT_SIZED(GhosttyPaste);
    paste.location = GHOSTTY_CLIPBOARD_LOCATION_STANDARD;
    paste.source = typed ? GHOSTTY_PASTE_SOURCE_TEXT : GHOSTTY_PASTE_SOURCE_CLIPBOARD;
    paste.mimes = &mime;
    paste.mimes_len = 1;
    paste.reader = GhosttyMimeReader{&readPaste, &source};
    // The window asks before a paste of several lines, so nothing is refused here.
    paste.allow_unsafe = true;
    ghostty_terminal_paste(m_terminal, &paste, nullptr);
}

void Terminal::reset()
{
    ghostty_terminal_reset(m_terminal);
}

// CAN first, so a sequence the program left unfinished cannot swallow these.
void Terminal::clearScrollback()
{
    write("\030\033[3J");
}

void Terminal::clearScreen()
{
    write("\030\033[H\033[2J\033[3J");
}

void Terminal::clearKeepingCursorLine()
{
    const int line = cursorLine();
    const int column = cursorColumn();
    std::string sequence = "\030\033[3J";
    // Deleting the lines above brings the cursor's line to the top.
    if (line > 0)
        sequence += "\033[1;1H\033[" + std::to_string(line) + "M";
    if (m_lines > 1)
        sequence += "\033[2;1H\033[J";
    sequence += "\033[1;" + std::to_string(column + 1) + "H";
    write(sequence);
}

GhosttyGridRef Terminal::viewportRef(int column, int line, bool* ok) const
{
    GhosttyPoint point{};
    point.tag = GHOSTTY_POINT_TAG_VIEWPORT;
    point.value.coordinate.x = uint16_t(std::clamp(column, 0, m_columns - 1));
    point.value.coordinate.y = uint32_t(std::clamp(line, 0, m_lines - 1));
    GhosttyGridRef ref{};
    *ok = ghostty_terminal_grid_ref(m_terminal, point, &ref) == GHOSTTY_SUCCESS;
    return ref;
}

static bool writeString(void* userdata, const uint8_t* data, size_t length)
{
    static_cast<std::string*>(userdata)->append(reinterpret_cast<const char*>(data), length);
    return true;
}

std::string Terminal::text(const GhosttySelection& selection) const
{
    GhosttyFormatter formatter = nullptr;
    if (ghostty_formatter_terminal_new(nullptr, &formatter, m_terminal) != GHOSTTY_SUCCESS)
        return {};
    const GhosttyFormatterFormat emit = GHOSTTY_FORMATTER_FORMAT_PLAIN;
    const bool yes = true;
    ghostty_formatter_set(formatter, GHOSTTY_FORMATTER_OPT_EMIT, &emit);
    ghostty_formatter_set(formatter, GHOSTTY_FORMATTER_OPT_UNWRAP, &yes);
    ghostty_formatter_set(formatter, GHOSTTY_FORMATTER_OPT_TRIM, &yes);
    ghostty_formatter_set(formatter, GHOSTTY_FORMATTER_OPT_SELECTION, &selection);
    std::string result;
    ghostty_formatter_format(formatter, GhosttyWriter{&writeString, &result});
    ghostty_formatter_free(formatter);
    return result;
}

bool Terminal::hasSelection() const
{
    GhosttySelection selection{};
    return ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_SELECTION, &selection) == GHOSTTY_SUCCESS;
}

std::string Terminal::selectedText() const
{
    GhosttySelection selection{};
    if (ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_SELECTION, &selection) != GHOSTTY_SUCCESS)
        return {};
    return text(selection);
}

void Terminal::setSelection(const GhosttySelection& selection)
{
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_SELECTION, &selection);
}

void Terminal::clearSelection()
{
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_SELECTION, nullptr);
}

void Terminal::selectAll()
{
    GhosttySelection selection{};
    if (ghostty_terminal_select_all(m_terminal, &selection) == GHOSTTY_SUCCESS)
        ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_SELECTION, &selection);
}

bool Terminal::selectWord(int column, int line)
{
    bool ok = false;
    GhosttyTerminalSelectWordOptions options = GHOSTTY_INIT_SIZED(GhosttyTerminalSelectWordOptions);
    options.ref = viewportRef(column, line, &ok);
    GhosttySelection selection{};
    if (!ok || ghostty_terminal_select_word(m_terminal, &options, &selection) != GHOSTTY_SUCCESS)
        return false;
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_SELECTION, &selection);
    return true;
}

bool Terminal::isSelected(int column, int line) const
{
    GhosttySelection selection{};
    if (ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_SELECTION, &selection) != GHOSTTY_SUCCESS)
        return false;
    GhosttyPoint point{};
    point.tag = GHOSTTY_POINT_TAG_VIEWPORT;
    point.value.coordinate.x = uint16_t(std::max(0, column));
    point.value.coordinate.y = uint32_t(std::max(0, line));
    bool contains = false;
    ghostty_terminal_selection_contains(m_terminal, &selection, point, &contains);
    return contains;
}

static bool rowFlag(const GhosttyGridRef& ref, GhosttyRowData data)
{
    GhosttyRow row = 0;
    bool value = false;
    if (ghostty_grid_ref_row(&ref, &row) == GHOSTTY_SUCCESS)
        ghostty_row_get(row, data, &value);
    return value;
}

Terminal::LogicalLine Terminal::logicalLine(int line) const
{
    LogicalLine result;
    if (line < 0 || line >= m_lines)
        return result;

    // Out to the rows the line wraps from and into, within the viewport.
    int first = line;
    bool ok = false;
    while (first > 0) {
        const GhosttyGridRef ref = viewportRef(0, first, &ok);
        if (!ok || !rowFlag(ref, GHOSTTY_ROW_DATA_WRAP_CONTINUATION))
            break;
        --first;
    }
    int last = line;
    while (last + 1 < m_lines) {
        const GhosttyGridRef ref = viewportRef(0, last, &ok);
        if (!ok || !rowFlag(ref, GHOSTTY_ROW_DATA_WRAP))
            break;
        ++last;
    }

    uint32_t codepoints[16];
    for (int row = first; row <= last; ++row) {
        for (int column = 0; column < m_columns; ++column) {
            const GhosttyGridRef ref = viewportRef(column, row, &ok);
            size_t count = 0;
            if (!ok)
                continue;
            GhosttyCell cell = 0;
            GhosttyCellWide wide = GHOSTTY_CELL_WIDE_NARROW;
            if (ghostty_grid_ref_cell(&ref, &cell) == GHOSTTY_SUCCESS)
                ghostty_cell_get(cell, GHOSTTY_CELL_DATA_WIDE, &wide);
            if (wide == GHOSTTY_CELL_WIDE_SPACER_TAIL || wide == GHOSTTY_CELL_WIDE_SPACER_HEAD)
                continue;
            const GhosttyResult read = ghostty_grid_ref_graphemes(&ref, codepoints, 16, &count);
            const size_t start = result.text.size();
            if (read != GHOSTTY_SUCCESS || count == 0)
                result.text += ' ';
            for (size_t i = 0; read == GHOSTTY_SUCCESS && i < count; ++i)
                cutefish::base::strings::appendUtf8(result.text, char32_t(codepoints[i]));
            result.cells.resize(result.text.size(), {row, column});
            std::fill(result.cells.begin() + long(start), result.cells.end(), std::pair<int, int>{row, column});
        }
    }
    return result;
}

std::string Terminal::hyperlinkAt(int column, int line) const
{
    bool ok = false;
    const GhosttyGridRef ref = viewportRef(column, line, &ok);
    if (!ok)
        return {};
    size_t length = 0;
    if (ghostty_grid_ref_hyperlink_uri(&ref, nullptr, 0, &length) != GHOSTTY_OUT_OF_SPACE || length == 0)
        return {};
    std::string uri(length, '\0');
    if (ghostty_grid_ref_hyperlink_uri(&ref, reinterpret_cast<uint8_t*>(uri.data()), length, &length) != GHOSTTY_SUCCESS)
        return {};
    uri.resize(length);
    return uri;
}

}
