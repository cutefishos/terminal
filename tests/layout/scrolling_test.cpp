// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <ctime>
#include <string>

#include "base/event_loop.h"
#include "base/file.h"
#include "ui/event.h"
#include "ui/scroll_bar.h"
#include "ui/test_input.h"
#include "ui/window.h"

#include "session.h"
#include "terminalview.h"

#include "terminalpage.h"
#include "terminalsettings.h"
#include "terminalwindow.h"
#include "windows.h"

using namespace cutefish;

static constexpr uint64_t kFrameNs = 16'666'667;

static std::string utf8(const std::u32string& text)
{
    std::string out;
    for (char32_t c : text) {
        if (c < 0x80) {
            out += char(c);
        } else if (c < 0x800) {
            out += char(0xc0 | (c >> 6));
            out += char(0x80 | (c & 0x3f));
        } else if (c < 0x10000) {
            out += char(0xe0 | (c >> 12));
            out += char(0x80 | ((c >> 6) & 0x3f));
            out += char(0x80 | (c & 0x3f));
        } else {
            out += char(0xf0 | (c >> 18));
            out += char(0x80 | ((c >> 12) & 0x3f));
            out += char(0x80 | ((c >> 6) & 0x3f));
            out += char(0x80 | (c & 0x3f));
        }
    }
    return out;
}

// A line of the viewport as the terminal holds it, as drawnText() gives it.
static std::u32string terminalText(const term::Terminal& terminal, int line)
{
    std::u32string text;
    for (int column = 0; column < terminal.columns(); ++column) {
        bool ok = false;
        const GhosttyGridRef ref = terminal.viewportRef(column, line, &ok);
        uint32_t codepoints[16];
        size_t count = 0;
        if (ok && ghostty_grid_ref_graphemes(&ref, codepoints, 16, &count) == GHOSTTY_SUCCESS && count > 0)
            text.append(reinterpret_cast<const char32_t*>(codepoints), count);
        else
            text.push_back(U' ');
    }
    text.erase(text.find_last_not_of(U' ') + 1);
    return text;
}

// What fastfetch prints, with output lines longer than the window: a
// coloured logo, then the cursor back up and the information beside it,
// lines that run past the right edge and wrap onto the next line.
static std::string fetchOutput(int columns, int run)
{
    static const char* const logo[] = {
        "        .-=+#%%@@@@@@%%#+=-.        ",
        "    .-+#@@@@@@#**+====+**#@@@@+:    ",
        "  .-*%@@@#+=:            .-*%@@*:   ",
        " .=%@@%=                    .=%@%=  ",
        " +#=    :*@@@+          +#=  :*@@+  ",
        " +#=    :*@@@+          +#=  :*@@+  ",
        " .=%@@%=                    .=%@%=  ",
        "  .-*%@@@#+=:            .-*%@@*:   ",
        "    .-+#@@@@@@#**+====+**#@@@@+:    ",
        "        .-=+#%%@@@@@@%%#+=-.        ",
    };
    const int logoLines = int(std::size(logo));
    std::string out = "\033[32mreion@reion-pc\033[0m:\033[34m~\033[0m$ fastfetch " + std::to_string(run) + "\r\n";
    for (int i = 0; i < logoLines; ++i)
        out += std::string("\033[1;3") + char('4' + i % 3) + "m" + logo[i] + "\033[0m\r\n";
    out += "\033[" + std::to_string(logoLines) + "A";
    for (int i = 0; i < logoLines; ++i) {
        std::string value = "run " + std::to_string(run) + " line " + std::to_string(i) + ":";
        // Every other line runs past the edge by a different amount.
        const int length = i % 2 ? columns - 40 + 10 + 3 * i : columns / 3;
        for (int k = 0; int(value.size()) < length; ++k)
            value += " " + std::to_string(k);
        out += "\033[40G\033[1;31mKey" + std::to_string(i) + "\033[0m: " + value + "\r\n";
    }
    out += "\r\n";
    return out;
}

class ScrollingTest : public testing::TestWithParam<double>
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_directory.isValid());
        TerminalWindow::Options options;
        options.startDirectory = m_directory.path();
        options.command = {"/bin/sleep", "30"};
        options.restoresSize = false;
        m_window = m_windows.create(std::move(options));
        m_page = m_window->currentPage();
        ASSERT_NE(m_page, nullptr);
        m_view = m_page->terminal();
        window().setScale(GetParam());
        window().grab();
        base::EventLoop::current()->dispatch();
        ASSERT_GT(m_view->columns(), 60);

        term::Terminal& terminal = m_page->session().terminal();
        for (int run = 0; run < 6; ++run)
            terminal.write(fetchOutput(m_view->columns(), run));
        m_page->session().outputReceived();
        base::EventLoop::current()->dispatch();
        present();
        ASSERT_GT(m_view->scrollbarMaximum(), 2 * m_view->lines());
        ASSERT_EQ(m_view->scrollbarValue(), m_view->scrollbarMaximum());
    }

    ui::Window& window() { return m_window->window(); }
    double scale() const { return GetParam(); }

    // A frame as the screen gets it, then the view's posted update, then
    // the next frame: input and a frame callback can come in one loop pass.
    ui::Image present()
    {
        ui::TestInput::presentFrame(window(), m_now += kFrameNs);
        base::EventLoop::current()->dispatch();
        return ui::TestInput::presentFrame(window(), m_now += kFrameNs);
    }

    // Each line shows the terminal's, the frame on screen equals the frame
    // painted whole, and the text keeps inside the grid's margins.
    void check(const std::string& step)
    {
        SCOPED_TRACE(step + ", scale " + std::to_string(scale()) + ", row "
                     + std::to_string(m_view->scrollbarValue()));
        const ui::Image presented = present();
        const ui::Image whole = window().grab(m_now);
        ASSERT_EQ(presented.width(), whole.width());
        ASSERT_EQ(presented.height(), whole.height());

        const term::Terminal& terminal = m_page->session().terminal();
        for (int line = 0; line < m_view->lines(); ++line)
            EXPECT_EQ(utf8(m_view->drawnText(line)), utf8(terminalText(terminal, line))) << "line " << line;

        int differing = 0;
        int firstY = -1;
        int lastY = -1;
        for (int y = 0; y < whole.height(); ++y) {
            for (int x = 0; x < whole.width(); ++x) {
                if (presented.pixel(x, y) != whole.pixel(x, y)) {
                    ++differing;
                    if (firstY < 0)
                        firstY = y;
                    lastY = y;
                }
            }
        }
        EXPECT_EQ(differing, 0) << "pixels the repaints left stale, device rows " << firstY << " to " << lastY;

        const ui::Rect ink = ui::TestInput::inkRect(*m_view, scale());
        const int margin = int(std::lround(scale()));
        const int height = int(std::lround(m_view->geometry().height * scale()));
        if (ink.width > 0) {
            EXPECT_GE(ink.y, margin) << "ink above the grid's top margin";
            EXPECT_LE(ink.y + ink.height, height - margin) << "ink below the grid's bottom margin";
        }
    }

    base::file::TemporaryDirectory m_directory{"/tmp", "terminal-scrolling-test"};
    TerminalSettings m_settings{m_directory.path() + "/terminal.conf"};
    Windows m_windows{m_settings};
    TerminalWindow* m_window = nullptr;
    TerminalPage* m_page = nullptr;
    term::TerminalView* m_view = nullptr;
    uint64_t m_now = [] {
        timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        return uint64_t(now.tv_sec) * 1'000'000'000 + uint64_t(now.tv_nsec);
    }();
};

TEST_P(ScrollingTest, WheelToTheTopRedrawsEveryLine)
{
    const ui::PointF centre = m_view->mapToWindow({m_view->geometry().width / 2, m_view->geometry().height / 2});
    check("at the bottom");
    for (int step = 0; m_view->scrollbarValue() > 0 && step < 200; ++step) {
        ui::ScrollEvent wheel;
        wheel.value120Y = -120;
        ui::TestInput::scroll(window(), centre, wheel);
        check("wheel " + std::to_string(step));
        if (HasFailure())
            return;
    }
    EXPECT_EQ(m_view->scrollbarValue(), 0);
}

TEST_P(ScrollingTest, TouchpadToTheTopRedrawsEveryLine)
{
    const ui::PointF centre = m_view->mapToWindow({m_view->geometry().width / 2, m_view->geometry().height / 2});
    const double line = m_view->cellMetrics().height / scale();
    for (int step = 0; m_view->scrollbarValue() > 0 && step < 400; ++step) {
        ui::ScrollEvent drag;
        drag.precise = true;
        drag.deltaY = -0.37 * line;
        drag.time = uint32_t(m_now / 1'000'000);
        ui::TestInput::scroll(window(), centre, drag);
        check("touchpad " + std::to_string(step));
        if (HasFailure())
            return;
    }
    ui::ScrollEvent stop;
    stop.precise = true;
    stop.stop = true;
    stop.time = uint32_t(m_now / 1'000'000);
    ui::TestInput::scroll(window(), centre, stop);
    for (int frame = 0; frame < 120; ++frame)
        present();
    check("touchpad at rest");
    EXPECT_EQ(m_view->scrollbarValue(), 0);
}

// The scroller moves the view in the frame callback, so the frame it moves
// in has to show the rows it moved to.
TEST_P(ScrollingTest, TouchpadFrameShowsTheRowsItScrolledTo)
{
    const ui::PointF centre = m_view->mapToWindow({m_view->geometry().width / 2, m_view->geometry().height / 2});
    const double line = m_view->cellMetrics().height / scale();
    const term::Terminal& terminal = m_page->session().terminal();
    for (int step = 0; step < 40; ++step) {
        ui::ScrollEvent drag;
        drag.precise = true;
        drag.deltaY = -0.37 * line;
        drag.time = uint32_t(m_now / 1'000'000);
        ui::TestInput::scroll(window(), centre, drag);
        ui::TestInput::presentFrame(window(), m_now += kFrameNs);
        SCOPED_TRACE("step " + std::to_string(step) + ", row " + std::to_string(m_view->scrollbarValue()));
        for (int line = 0; line < m_view->lines(); ++line)
            ASSERT_EQ(utf8(m_view->drawnText(line)), utf8(terminalText(terminal, line))) << "line " << line;
    }
}

// Output that moves the history under a touchpad scroll leaves the
// rows under the fingers where they are.
TEST_P(ScrollingTest, OutputUnderATouchpadScrollKeepsTheRowsInPlace)
{
    const ui::PointF centre = m_view->mapToWindow({m_view->geometry().width / 2, m_view->geometry().height / 2});
    const double line = m_view->cellMetrics().height / scale();
    term::Session& session = m_page->session();
    // History over several pages, so the rows in view outlive the oldest.
    for (int i = 0; i < 3000; ++i)
        session.terminal().write("Setting up package " + std::to_string(i) + " ...\r\n");
    session.outputReceived();
    present();
    session.setScrollbackLines(m_view->scrollbarMaximum());
    const auto drag = [&] {
        ui::ScrollEvent event;
        event.precise = true;
        event.deltaY = -0.25 * line;
        event.time = uint32_t(m_now / 1'000'000);
        ui::TestInput::scroll(window(), centre, event);
        present();
    };
    for (int step = 0; step < 8; ++step)
        drag();
    const std::u32string top = m_view->drawnText(0);
    const int row = m_view->scrollbarValue();
    ASSERT_LT(row, m_view->scrollbarMaximum());

    // The history is full, and drops its oldest rows a page at a time.
    for (int step = 0; m_view->scrollbarValue() == row && step < 400; ++step) {
        for (int i = 0; i < 50; ++i)
            session.terminal().write("Unpacking package " + std::to_string(step * 50 + i) + " ...\r\n");
        session.outputReceived();
        present();
    }
    ASSERT_LT(m_view->scrollbarValue(), row);
    ASSERT_EQ(utf8(m_view->drawnText(0)), utf8(top));

    // A quarter of a line at a time: the top row moves down by at most one.
    for (int step = 0; step < 4; ++step) {
        drag();
        const bool kept = m_view->drawnText(0) == top || m_view->drawnText(1) == top;
        EXPECT_TRUE(kept) << "step " << step << ": " << utf8(m_view->drawnText(0));
    }
}

TEST_P(ScrollingTest, OutputKeepsATouchpadScrollGoing)
{
    const ui::PointF centre = m_view->mapToWindow({m_view->geometry().width / 2, m_view->geometry().height / 2});
    const double line = m_view->cellMetrics().height / scale();
    const int start = m_view->scrollbarValue();
    term::Session& session = m_page->session();
    for (int step = 0; step < 40; ++step) {
        ui::ScrollEvent drag;
        drag.precise = true;
        drag.deltaY = -0.5 * line;
        drag.time = uint32_t(m_now / 1'000'000);
        ui::TestInput::scroll(window(), centre, drag);
        session.terminal().write("Unpacking package " + std::to_string(step) + " ...\r\n");
        session.outputReceived();
        present();
    }
    EXPECT_LE(m_view->scrollbarValue(), start - 15);
}

TEST_P(ScrollingTest, ScrollBarDragToTheTopRedrawsEveryLine)
{
    ui::ScrollBar* bar = nullptr;
    for (const auto& child : m_page->children()) {
        if (auto* found = dynamic_cast<ui::ScrollBar*>(child.get()))
            bar = found;
    }
    ASSERT_NE(bar, nullptr);
    const ui::RectF track = bar->geometry();
    // The handle sits at the end of the track while the view is at the bottom.
    const ui::PointF start = m_page->mapToWindow({track.x + track.width / 2, track.bottom() - 20});
    ui::TestInput::movePointer(window(), start);
    ui::TestInput::press(window(), start);
    const int steps = 30;
    for (int step = 1; step <= steps; ++step) {
        ui::TestInput::movePointer(window(), {start.x, start.y - (start.y + 20) * step / steps});
        check("drag " + std::to_string(step));
        if (HasFailure())
            return;
    }
    ui::TestInput::release(window(), {start.x, 0});
    check("dropped");
    EXPECT_EQ(m_view->scrollbarValue(), 0);
}

INSTANTIATE_TEST_SUITE_P(Scales, ScrollingTest, testing::Values(1.75, 1.0));
