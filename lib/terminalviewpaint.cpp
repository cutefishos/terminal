// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include <algorithm>
#include <cmath>

#include "base/event_loop.h"
#include "ui/painter.h"
#include "ui/window.h"

#include "session.h"
#include "terminalview.h"

namespace ui = cutefish::ui;
using cutefish::base::Rect;

namespace term {

void TerminalView::scheduleUpdate()
{
    if (m_updatePending)
        return;
    m_updatePending = true;
    std::weak_ptr<bool> alive = m_alive;
    cutefish::base::EventLoop::current()->post([this, alive] {
        if (alive.lock())
            updateRenderState();
    });
}

// Takes what changed in the terminal into the render state and asks for the
// rows it touched to be drawn again; paint() only reads the render state.
void TerminalView::updateRenderState()
{
    m_updatePending = false;
    Terminal* terminal = this->terminal();
    if (!terminal)
        return;
    if (!m_held)
        ghostty_render_state_update(m_state, terminal->handle());
    scrollChanged();
    if (terminal->tracksMouse() != m_tracksMouse) {
        m_tracksMouse = !m_tracksMouse;
        updatePointerShape();
    }

    GhosttyRenderStateDirty dirty = GHOSTTY_RENDER_STATE_DIRTY_FALSE;
    ghostty_render_state_get(m_state, GHOSTTY_RENDER_STATE_DATA_DIRTY, &dirty);
    if (dirty == GHOSTTY_RENDER_STATE_DIRTY_FULL) {
        requestPaint();
    } else if (dirty == GHOSTTY_RENDER_STATE_DIRTY_PARTIAL) {
        ghostty_render_state_get(m_state, GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR, &m_rows);
        while (ghostty_render_state_row_iterator_next(m_rows)) {
            bool rowDirty = false;
            int32_t line = 0;
            ghostty_render_state_row_get(m_rows, GHOSTTY_RENDER_STATE_ROW_DATA_DIRTY, &rowDirty);
            if (!rowDirty)
                continue;
            ghostty_render_state_row_get(m_rows, GHOSTTY_RENDER_STATE_ROW_DATA_VIEWPORT_Y, &line);
            requestPaint(rowRect(line));
        }
    }

    GhosttyRenderStateCursor cursor = GHOSTTY_INIT_SIZED(GhosttyRenderStateCursor);
    ghostty_render_state_get(m_state, GHOSTTY_RENDER_STATE_DATA_CURSOR, &cursor);
    const std::pair<int, int> position = cursor.viewport_has_value
            ? std::pair<int, int>{cursor.viewport_x, cursor.viewport_y}
            : std::pair<int, int>{-1, -1};
    if (position != m_lastCursor) {
        if (m_lastCursor.second >= 0)
            requestPaint(rowRect(m_lastCursor.second));
        if (position.second >= 0)
            requestPaint(rowRect(position.second));
        m_lastCursor = position;
        restartCursorBlink();
    }
    ghostty_render_state_clean(m_state);
}

static void fillDevice(ui::Painter& painter, const Rect& rect, ui::Color color)
{
    ui::fillDeviceRect(painter, ui::Rect{rect.x(), rect.y(), rect.width(), rect.height()}, color);
}

static ui::Color colorOf(GhosttyColorRgb rgb)
{
    return ui::Color{rgb.r, rgb.g, rgb.b, 255};
}

static ui::Color styleColor(const GhosttyStyleColor& color, const GhosttyRenderStateColors& colors)
{
    if (color.tag == GHOSTTY_STYLE_COLOR_RGB)
        return colorOf(color.value.rgb);
    return colorOf(colors.palette[color.value.palette]);
}

struct PaintFrame
{
    ui::Painter& painter;
    // The glyphs of a row, drawn in runs of one style and colour.
    struct Glyph
    {
        size_t text;
        int style;
        ui::Color color;
        int x;
        int baseline;
    };
    std::vector<std::u32string> texts;
    std::vector<Glyph> glyphs;
    // Filled over the glyphs: box drawing, decorations, thin cursors.
    std::vector<std::pair<Rect, ui::Color>> over;

    void fill(const Rect& rect, ui::Color color)
    {
        if (rect.width() > 0 && rect.height() > 0 && color.a > 0)
            fillDevice(painter, rect, color);
    }
    void fillOver(const Rect& rect, ui::Color color)
    {
        if (rect.width() > 0 && rect.height() > 0 && color.a > 0)
            over.emplace_back(rect, color);
    }
};

void TerminalView::paint(ui::Painter& painter)
{
    // The window repaints its focus view when the keyboard comes or goes.
    if (hasKeyboard()) {
        if (m_blinkingCursor && !m_cursorBlinkTimer.isActive())
            m_cursorBlinkTimer.start();
    } else if (m_cursorBlinkTimer.isActive()) {
        m_cursorBlinkTimer.stop();
        m_cursorHidden = false;
    }
    if (painter.scale() != m_font.scale())
        updateGeometry(painter.scale());

    if (m_opacity > 0) {
        const double s = painter.scale();
        fillDevice(painter,
                           Rect(0, 0, int(std::lround(geometry().width * s)), int(std::lround(geometry().height * s))),
                           m_background.withAlpha(m_opacity));
    }
    if (!terminal())
        return;
    // Rows shifted by a touchpad scroll, and the row below the viewport,
    // reach past the grid's margins: the header and the window's edge are
    // not the view's to draw over.
    const double margin = gridTop() / m_font.scale();
    painter.save();
    painter.clip({margin, margin, geometry().width - 2 * margin, geometry().height - 2 * margin});
    paintRows(painter);
    paintPreedit(painter);
    painter.restore();
}

void TerminalView::paintRows(ui::Painter& painter)
{
    const CellMetrics& cell = m_font.metrics();
    const double s = m_font.scale();
    const int left = gridLeft();
    const int top = gridTop() - int(std::lround(m_scrollOffset * s));
    const ui::RectF clip = painter.clipBounds();
    const int clipTop = int(std::floor(clip.y * s));
    const int clipBottom = int(std::ceil((clip.y + clip.height) * s));
    const int light = std::max(1, int(std::lround(s)));

    GhosttyRenderStateColors colors = GHOSTTY_INIT_SIZED(GhosttyRenderStateColors);
    ghostty_render_state_get(m_state, GHOSTTY_RENDER_STATE_DATA_COLORS, &colors);
    const ui::Color defaultForeground = colorOf(colors.foreground);
    const ui::Color defaultBackground = colorOf(colors.background);
    GhosttyRenderStateCursor cursor = GHOSTTY_INIT_SIZED(GhosttyRenderStateCursor);
    ghostty_render_state_get(m_state, GHOSTTY_RENDER_STATE_DATA_CURSOR, &cursor);
    const bool focused = hasKeyboard();
    const bool showCursor = cursor.visible && cursor.viewport_has_value && m_preedit.empty()
            && (!focused || !m_cursorHidden);

    PaintFrame frame{painter, {}, {}, {}};
    frame.texts.reserve(size_t(m_columns) * size_t(m_lines + 2));
    bool blinkingText = false;
    uint32_t codepoints[16];

    ghostty_render_state_get(m_state, GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR, &m_rows);
    while (ghostty_render_state_row_iterator_next(m_rows)) {
        int32_t line = 0;
        ghostty_render_state_row_get(m_rows, GHOSTTY_RENDER_STATE_ROW_DATA_VIEWPORT_Y, &line);
        const int rowTop = top + line * cell.height;
        if (rowTop >= clipBottom || rowTop + cell.height <= clipTop)
            continue;
        GhosttyRenderStateRowSelection selection = GHOSTTY_INIT_SIZED(GhosttyRenderStateRowSelection);
        const bool hasSelection = ghostty_render_state_row_get(m_rows, GHOSTTY_RENDER_STATE_ROW_DATA_SELECTION,
                                                                &selection) == GHOSTTY_SUCCESS;
        ghostty_render_state_row_get(m_rows, GHOSTTY_RENDER_STATE_ROW_DATA_CELLS, &m_cells);

        for (int column = 0; ghostty_render_state_row_cells_next(m_cells); ++column) {
            GhosttyCell raw = 0;
            ghostty_render_state_row_cells_get(m_cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_RAW, &raw);
            GhosttyCellWide wide = GHOSTTY_CELL_WIDE_NARROW;
            ghostty_cell_get(raw, GHOSTTY_CELL_DATA_WIDE, &wide);
            if (wide == GHOSTTY_CELL_WIDE_SPACER_TAIL || wide == GHOSTTY_CELL_WIDE_SPACER_HEAD)
                continue;
            const int span = wide == GHOSTTY_CELL_WIDE_WIDE ? 2 : 1;
            const Rect box(left + column * cell.width, rowTop, cell.width * span, cell.height);

            GhosttyStyle style;
            bool styled = false;
            ghostty_render_state_row_cells_get(m_cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_HAS_STYLING, &styled);
            if (styled)
                ghostty_render_state_row_cells_get(m_cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_STYLE, &style);
            else
                ghostty_style_default(&style);

            GhosttyColorRgb rgb;
            ui::Color foreground = ghostty_render_state_row_cells_get(m_cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_FG_COLOR,
                                                                       &rgb) == GHOSTTY_SUCCESS
                    ? colorOf(rgb)
                    : defaultForeground;
            bool hasBackground = ghostty_render_state_row_cells_get(m_cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_BG_COLOR,
                                                                    &rgb) == GHOSTTY_SUCCESS;
            ui::Color background = hasBackground ? colorOf(rgb) : defaultBackground;

            const bool selected = hasSelection && column >= selection.start_x && column <= selection.end_x;
            if (bool(style.flags & GHOSTTY_STYLE_FLAG_INVERSE) != selected) {
                std::swap(foreground, background);
                hasBackground = true;
            }
            // The default background is the window's own, translucent or not.
            if (hasBackground && background != defaultBackground)
                frame.fill(box, background);

            ui::Color textColor = foreground;
            if (showCursor && line == cursor.viewport_y && column == cursor.viewport_x) {
                const ui::Color color = colors.cursor_has_value ? colorOf(colors.cursor) : foreground;
                const GhosttyCursorStyle shape = focused ? cursor.visual_style : GHOSTTY_CURSOR_STYLE_BLOCK_HOLLOW;
                if (shape == GHOSTTY_CURSOR_STYLE_BLOCK) {
                    frame.fill(box, color);
                    textColor = background;
                } else if (shape == GHOSTTY_CURSOR_STYLE_UNDERLINE) {
                    frame.fillOver(Rect(box.x(), box.bottom() - light, box.width(), light), color);
                } else if (shape == GHOSTTY_CURSOR_STYLE_BAR) {
                    frame.fillOver(Rect(box.x(), box.y(), light, box.height()), color);
                } else {
                    frame.fillOver(Rect(box.x(), box.y(), box.width(), light), color);
                    frame.fillOver(Rect(box.x(), box.bottom() - light, box.width(), light), color);
                    frame.fillOver(Rect(box.x(), box.y(), light, box.height()), color);
                    frame.fillOver(Rect(box.right() - light, box.y(), light, box.height()), color);
                }
            }

            if (style.flags & GHOSTTY_STYLE_FLAG_BLINK) {
                blinkingText = true;
                if (m_textHidden)
                    continue;
            }
            if (style.flags & GHOSTTY_STYLE_FLAG_INVISIBLE)
                continue;
            if (style.flags & GHOSTTY_STYLE_FLAG_FAINT)
                textColor = textColor.withAlpha(0.5);

            const int baseline = rowTop + cell.baseline;
            const int underline = GHOSTTY_STYLE_UNDERLINE(style);
            if (underline) {
                const ui::Color color = style.underline_color.tag != GHOSTTY_STYLE_COLOR_NONE
                        ? styleColor(style.underline_color, colors)
                        : textColor;
                const int y = baseline + cell.underline;
                frame.fillOver(Rect(box.x(), y, box.width(), cell.lineWidth), color);
                // Double, and the curly, dotted and dashed styles, as a second line below.
                if (underline >= 2)
                    frame.fillOver(Rect(box.x(), y + 2 * cell.lineWidth, box.width(), cell.lineWidth), color);
            }
            if (style.flags & GHOSTTY_STYLE_FLAG_STRIKETHROUGH)
                frame.fillOver(Rect(box.x(), baseline - cell.strikeout, box.width(), cell.lineWidth), textColor);
            if (style.flags & GHOSTTY_STYLE_FLAG_OVERLINE)
                frame.fillOver(Rect(box.x(), rowTop, box.width(), cell.lineWidth), textColor);

            GhosttyCodepointBuffer graphemes{codepoints, 16, 0};
            ghostty_render_state_row_cells_get(m_cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_BUF, &graphemes);
            const size_t length = std::min<size_t>(graphemes.len, 16);
            if (length == 0 || (length == 1 && codepoints[0] == ' '))
                continue;
            if (length == 1 && BoxDrawing::covers(char32_t(codepoints[0]))) {
                const BoxDrawing::Shape& shape = m_boxes.shape(char32_t(codepoints[0]), box.width(), box.height(), light);
                if (shape.shade > 0)
                    frame.fillOver(box, textColor.withAlpha(textColor.a / 255.0 * shape.shade));
                for (const Rect& rect : shape.rects)
                    frame.fillOver(Rect(box.x() + rect.x(), box.y() + rect.y(), rect.width(), rect.height()), textColor);
                continue;
            }
            int fontStyle = CellFont::Regular;
            if (style.flags & GHOSTTY_STYLE_FLAG_BOLD)
                fontStyle |= CellFont::Bold;
            if (style.flags & GHOSTTY_STYLE_FLAG_ITALIC)
                fontStyle |= CellFont::Italic;
            frame.texts.emplace_back(reinterpret_cast<const char32_t*>(codepoints), length);
            frame.glyphs.push_back({frame.texts.size() - 1, fontStyle, textColor, box.x(), baseline});
        }
    }

    // One call for each run of one style, colour and row.
    std::vector<ui::GlyphCluster> run;
    for (size_t i = 0; i < frame.glyphs.size();) {
        const PaintFrame::Glyph& first = frame.glyphs[i];
        run.clear();
        size_t end = i;
        while (end < frame.glyphs.size() && frame.glyphs[end].style == first.style
               && frame.glyphs[end].color == first.color && frame.glyphs[end].baseline == first.baseline) {
            run.push_back({frame.texts[frame.glyphs[end].text], double(frame.glyphs[end].x)});
            ++end;
        }
        m_font.draw(painter, first.style, first.color, first.baseline, run);
        i = end;
    }
    for (const auto& [rect, color] : frame.over)
        fillDevice(painter, rect, color);

    for (const Segment& segment : m_linkSegments) {
        const int y = top + segment.line * cell.height + cell.baseline + cell.underline;
        frame.fill(Rect(left + segment.column * cell.width, y, segment.length * cell.width, cell.lineWidth), m_linkColor);
    }

    if (blinkingText != m_hasBlinkingText) {
        m_hasBlinkingText = blinkingText;
        if (blinkingText && !m_textBlinkTimer.isActive())
            m_textBlinkTimer.start();
        if (!blinkingText) {
            m_textBlinkTimer.stop();
            m_textHidden = false;
        }
    }
}

std::u32string TerminalView::drawnText(int line) const
{
    GhosttyRenderStateRowIterator rows = m_rows;
    GhosttyRenderStateRowCells cells = m_cells;
    ghostty_render_state_get(m_state, GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR, &rows);
    while (ghostty_render_state_row_iterator_next(rows)) {
        int32_t y = 0;
        ghostty_render_state_row_get(rows, GHOSTTY_RENDER_STATE_ROW_DATA_VIEWPORT_Y, &y);
        if (y != line)
            continue;
        std::u32string text;
        ghostty_render_state_row_get(rows, GHOSTTY_RENDER_STATE_ROW_DATA_CELLS, &cells);
        uint32_t codepoints[16];
        while (ghostty_render_state_row_cells_next(cells)) {
            GhosttyCodepointBuffer graphemes{codepoints, 16, 0};
            ghostty_render_state_row_cells_get(cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_BUF, &graphemes);
            if (graphemes.len == 0)
                text.push_back(U' ');
            else
                text.append(reinterpret_cast<const char32_t*>(codepoints), std::min<size_t>(graphemes.len, 16));
        }
        text.erase(text.find_last_not_of(U' ') + 1);
        return text;
    }
    return {};
}

// The input method's text over the cells at the cursor, underlined.
void TerminalView::paintPreedit(ui::Painter& painter)
{
    if (m_preedit.empty() || m_lastCursor.second < 0)
        return;
    const CellMetrics& cell = m_font.metrics();
    const int top = gridTop() - int(std::lround(m_scrollOffset * m_font.scale())) + m_lastCursor.second * cell.height;
    int x = gridLeft() + m_lastCursor.first * cell.width;
    std::vector<std::u32string> texts;
    std::vector<int> positions;
    for (size_t i = 0; i < m_preedit.size();) {
        std::u32string cluster(1, m_preedit[i]);
        const int width = std::max<int>(1, ghostty_unicode_codepoint_width(uint32_t(m_preedit[i])));
        for (++i; i < m_preedit.size() && ghostty_unicode_codepoint_width(uint32_t(m_preedit[i])) == 0; ++i)
            cluster.push_back(m_preedit[i]);
        texts.push_back(std::move(cluster));
        positions.push_back(x);
        x += width * cell.width;
    }
    const int start = gridLeft() + m_lastCursor.first * cell.width;
    fillDevice(painter, Rect(start, top, x - start, cell.height), m_background);
    std::vector<ui::GlyphCluster> run;
    for (size_t i = 0; i < texts.size(); ++i)
        run.push_back({texts[i], double(positions[i])});
    m_font.draw(painter, CellFont::Regular, m_foreground, top + cell.baseline, run);
    fillDevice(painter, Rect(start, top + cell.baseline + cell.underline, x - start, cell.lineWidth),
                       m_foreground);
}

}
