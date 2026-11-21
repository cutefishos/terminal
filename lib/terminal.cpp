// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include "terminal.h"

#include <algorithm>
#include <cstdint>
#include <optional>

#include "base/strings.h"
#include "base/url.h"

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
    notePromptMarks(output);
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

// libghostty-vt reads OSC 133 but does not tell whether a shell marks its
// prompts, or what it does with clicks there: the cl and click_events
// options of A and N, which hold until another prompt sets them.
void Terminal::notePromptMarks(std::string_view output)
{
    static constexpr std::string_view kStart = "\033]133;";
    std::string joined;
    if (!m_pendingMark.empty()) {
        joined = std::exchange(m_pendingMark, std::string()) + std::string(output);
        output = joined;
    }
    size_t at = 0;
    while ((at = output.find(kStart, at)) != std::string_view::npos) {
        const size_t begin = at + kStart.size();
        const size_t end = output.find_first_of("\a\033", begin);
        if (end == std::string_view::npos) {
            // Cut by the end of this output; a long one is not a mark.
            if (output.size() - at < 256)
                m_pendingMark = output.substr(at);
            return;
        }
        m_promptMarks = true;
        notePromptMark(output.substr(begin, end - begin));
        at = end;
    }
    for (size_t length = std::min(kStart.size() - 1, output.size()); length > 0; --length) {
        if (kStart.starts_with(output.substr(output.size() - length))) {
            m_pendingMark = output.substr(output.size() - length);
            break;
        }
    }
}

void Terminal::notePromptMark(std::string_view mark)
{
    if (!mark.starts_with('A') && !mark.starts_with('N'))
        return;
    std::optional<PromptClick> events;
    std::optional<PromptClick> line;
    for (const std::string_view option : cutefish::base::strings::splitViews(mark, ';')) {
        if (option == "click_events=1")
            events = PromptClick::AbsoluteEvents;
        else if (option == "click_events=2")
            events = PromptClick::RelativeEvents;
        else if (option.starts_with("cl="))
            line = PromptClick::Line;
    }
    if (events)
        m_promptClick = *events;
    else if (line)
        m_promptClick = *line;
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

bool Terminal::cursorAtPrompt() const
{
    bool atPrompt = false;
    ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_CURSOR_AT_PROMPT, &atPrompt);
    return atPrompt;
}

std::string Terminal::reportedDirectory() const
{
    GhosttyString pwd{};
    if (ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_PWD, &pwd) != GHOSTTY_SUCCESS || !pwd.ptr)
        return {};
    std::string_view text(reinterpret_cast<const char*>(pwd.ptr), pwd.len);
    // OSC 7 gives a URL, file:// or kitty-shell-cwd://, whose host part is
    // the shell's machine.
    bool encoded = false;
    for (const std::string_view scheme : {"file://", "kitty-shell-cwd://"}) {
        if (text.starts_with(scheme)) {
            encoded = scheme == "file://";
            text.remove_prefix(scheme.size());
            const size_t slash = text.find('/');
            text = slash == std::string_view::npos ? std::string_view() : text.substr(slash);
            break;
        }
    }
    if (!text.starts_with('/'))
        return {};
    return encoded ? cutefish::base::url::percentDecoded(text) : std::string(text);
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

void Terminal::scrollToPrompt(int prompts)
{
    GhosttyTerminalScrollViewport scroll{};
    scroll.tag = GHOSTTY_TERMINAL_SCROLL_VIEWPORT_DELTA_PROMPT;
    scroll.value.delta_prompt = prompts;
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

bool Terminal::paste(std::string_view text, bool typed, bool allowUnsafe)
{
    if (text.empty())
        return true;
    PasteSource source{text};
    const GhosttyString mime{reinterpret_cast<const uint8_t*>("text/plain"), 10};
    GhosttyPaste paste = GHOSTTY_INIT_SIZED(GhosttyPaste);
    paste.location = GHOSTTY_CLIPBOARD_LOCATION_STANDARD;
    paste.source = typed ? GHOSTTY_PASTE_SOURCE_TEXT : GHOSTTY_PASTE_SOURCE_CLIPBOARD;
    paste.mimes = &mime;
    paste.mimes_len = 1;
    paste.reader = GhosttyMimeReader{&readPaste, &source};
    paste.allow_unsafe = allowUnsafe;
    return ghostty_terminal_paste(m_terminal, &paste, nullptr) != GHOSTTY_REJECTED;
}

void Terminal::reset()
{
    ghostty_terminal_reset(m_terminal);
    // Colours a program set outlive the reset; the scheme's come back.
    write("\033]104\033\\\033]110\033\\\033]111\033\\\033]112\033\\");
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

void Terminal::clearAboveCursor()
{
    const int line = cursorLine();
    const int column = cursorColumn();
    std::string sequence = "\030\033[3J";
    // From the top to the end of the line above.
    if (line > 0)
        sequence += "\033[" + std::to_string(line) + ";" + std::to_string(m_columns) + "H\033[1J";
    sequence += "\033[" + std::to_string(line + 1) + ";" + std::to_string(column + 1) + "H";
    write(sequence);
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

// A cell of the whole screen, scrollback included, from 0 at its top.
struct ScreenCell
{
    GhosttyCellSemanticContent content = GHOSTTY_CELL_SEMANTIC_OUTPUT;
    GhosttyRow row = 0;
    bool ok = false;
};

static ScreenCell screenCell(GhosttyTerminal terminal, int column, uint64_t line)
{
    GhosttyPoint point{};
    point.tag = GHOSTTY_POINT_TAG_SCREEN;
    point.value.coordinate.x = uint16_t(column);
    point.value.coordinate.y = uint32_t(line);
    GhosttyGridRef ref{};
    ScreenCell result;
    if (ghostty_terminal_grid_ref(terminal, point, &ref) != GHOSTTY_SUCCESS)
        return result;
    GhosttyCell cell = 0;
    result.ok = ghostty_grid_ref_cell(&ref, &cell) == GHOSTTY_SUCCESS
            && ghostty_grid_ref_row(&ref, &result.row) == GHOSTTY_SUCCESS;
    if (result.ok)
        ghostty_cell_get(cell, GHOSTTY_CELL_DATA_SEMANTIC_CONTENT, &result.content);
    return result;
}

static bool rowFlag(GhosttyRow row, GhosttyRowData data)
{
    bool value = false;
    ghostty_row_get(row, data, &value);
    return value;
}

static GhosttyRowSemanticPrompt rowPrompt(GhosttyRow row)
{
    GhosttyRowSemanticPrompt prompt = GHOSTTY_ROW_SEMANTIC_NONE;
    ghostty_row_get(row, GHOSTTY_ROW_DATA_SEMANTIC_PROMPT, &prompt);
    return prompt;
}

// Ported from the reference terminal's prompt click (Screen.promptClickLine);
// see the README. Arrow keys only cross the input cells of one command line,
// soft-wrapped over rows or continued on rows the shell marked.
std::string Terminal::promptClickInput(int column, int line) const
{
    if (m_promptClick == PromptClick::None || !cursorAtPrompt())
        return {};
    const Scrollbar bar = scrollbar();
    const uint64_t clickRow = bar.offset + uint64_t(std::max(0, line));
    const uint64_t cursorRow = bar.total - uint64_t(m_lines) + uint64_t(cursorLine());
    const int cursorX = cursorColumn();

    // The prompt the cursor is at; a click above it moves nothing.
    uint64_t promptRow = cursorRow;
    while (true) {
        const ScreenCell cell = screenCell(m_terminal, 0, promptRow);
        if (cell.ok && rowPrompt(cell.row) == GHOSTTY_ROW_SEMANTIC_PROMPT)
            break;
        if (promptRow == 0)
            return {};
        --promptRow;
    }
    if (clickRow < promptRow)
        return {};

    if (m_promptClick != PromptClick::Line) {
        const uint64_t y = m_promptClick == PromptClick::AbsoluteEvents ? uint64_t(line) + 1 : clickRow - promptRow + 1;
        return "\033[<0;" + std::to_string(column + 1) + ";" + std::to_string(y) + "M";
    }

    if (clickRow == cursorRow && column == cursorX)
        return {};
    const std::string right = mode(GHOSTTY_MODE_CURSOR_KEYS) ? "\033OC" : "\033[C";
    const std::string left = mode(GHOSTTY_MODE_CURSOR_KEYS) ? "\033OD" : "\033[D";
    const bool cursorOnInput = screenCell(m_terminal, cursorX, cursorRow).content == GHOSTTY_CELL_SEMANTIC_INPUT;
    size_t count = 0;
    std::string keys;

    if (cursorRow < clickRow || (cursorRow == clickRow && cursorX < column)) {
        for (uint64_t row = cursorRow; row <= clickRow; ++row) {
            const ScreenCell first = screenCell(m_terminal, 0, row);
            if (!first.ok)
                break;
            if (row != cursorRow && rowPrompt(first.row) != GHOSTTY_ROW_SEMANTIC_PROMPT_CONTINUATION)
                break;
            int start = m_columns;
            if (row == cursorRow) {
                start = cursorX + 1;
            } else {
                for (int x = 0; x < m_columns; ++x) {
                    if (screenCell(m_terminal, x, row).content == GHOSTTY_CELL_SEMANTIC_INPUT) {
                        start = x;
                        break;
                    }
                }
            }
            bool found = false;
            for (int x = start; x < m_columns; ++x) {
                if (screenCell(m_terminal, x, row).content != GHOSTTY_CELL_SEMANTIC_INPUT)
                    continue;
                ++count;
                if (row == clickRow && x == column) {
                    found = true;
                    break;
                }
            }
            if (found)
                break;
            if (!rowFlag(first.row, GHOSTTY_ROW_DATA_WRAP)) {
                // Past the end: one more, to the empty cell after the text.
                if (cursorOnInput)
                    ++count;
                break;
            }
        }
        for (size_t i = 0; i < count; ++i)
            keys += right;
        return keys;
    }

    for (uint64_t row = cursorRow + 1; row-- > clickRow;) {
        const ScreenCell first = screenCell(m_terminal, 0, row);
        if (!first.ok)
            break;
        const int end = row == cursorRow ? cursorX : m_columns;
        bool found = false;
        for (int x = end; x-- > 0;) {
            if (screenCell(m_terminal, x, row).content != GHOSTTY_CELL_SEMANTIC_INPUT)
                continue;
            ++count;
            if (row == clickRow && x == column) {
                found = true;
                break;
            }
        }
        if (found || !rowFlag(first.row, GHOSTTY_ROW_DATA_WRAP_CONTINUATION))
            break;
    }
    for (size_t i = 0; i < count; ++i)
        keys += left;
    return keys;
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
