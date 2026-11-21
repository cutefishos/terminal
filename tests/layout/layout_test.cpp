// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <chrono>
#include <clocale>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include <xkbcommon/xkbcommon-keysyms.h>

#include "base/event_loop.h"
#include "base/file.h"
#include "base/timer.h"
#include "ui/application.h"
#include "ui/event.h"
#include "ui/test_input.h"
#include "ui/view.h"
#include "ui/window.h"

#include "session.h"
#include "terminalview.h"

#include "terminalpage.h"
#include "terminalsettings.h"
#include "terminalwindow.h"
#include "windows.h"

using namespace cutefish;

// Runs the loop until the condition holds, for at most five seconds.
static bool waitFor(const std::function<bool()>& condition)
{
    base::EventLoop& loop = *base::EventLoop::current();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!condition()) {
        if (std::chrono::steady_clock::now() > deadline)
            return false;
        base::Timer tick;
        tick.setSingleShot(true);
        tick.onTimeout = [] {};
        tick.start(std::chrono::milliseconds(10));
        loop.dispatch();
    }
    return true;
}

class WindowTest : public testing::Test
{
protected:
    void SetUp() override { ASSERT_TRUE(m_directory.isValid()); }

    TerminalWindow::Options shell() const
    {
        TerminalWindow::Options options;
        options.startDirectory = m_directory.path();
        options.command = {"/bin/sh"};
        options.restoresSize = false;
        return options;
    }

    base::file::TemporaryDirectory m_directory{"/tmp", "terminal-window-test"};
    TerminalSettings m_settings{m_directory.path() + "/terminal.conf"};
};

TEST_F(WindowTest, TabLeavingAWindowIsNoLongerHeard)
{
    Windows windows(m_settings);
    TerminalWindow* first = windows.create(shell());
    TerminalWindow* second = windows.create(shell());
    first->openTab(m_directory.path(), {"/bin/sh"});
    ASSERT_EQ(first->tabCount(), 2);

    std::unique_ptr<TerminalPage> page = first->takeTab(1);
    ASSERT_TRUE(page);
    EXPECT_TRUE(page->titleChanged.isEmpty());
    EXPECT_TRUE(page->terminalClosed.isEmpty());

    TerminalPage* moved = page.get();
    second->insertTab(std::move(page), 1);
    EXPECT_FALSE(moved->titleChanged.isEmpty());
    EXPECT_FALSE(moved->terminalClosed.isEmpty());
    EXPECT_EQ(second->tabCount(), 2);
}

TEST_F(WindowTest, CursorBlinksOnlyWithTheKeyboard)
{
    Windows windows(m_settings);
    TerminalWindow* window = windows.create(shell());
    ui::Window& surface = window->window();
    term::TerminalView* terminal = window->currentPage()->terminal();
    ASSERT_TRUE(terminal->hasFocus());

    ui::TestInput::setKeyboardFocus(surface, true);
    surface.grab();
    EXPECT_TRUE(terminal->isCursorBlinking());

    ui::TestInput::setKeyboardFocus(surface, false);
    surface.grab();
    EXPECT_FALSE(terminal->isCursorBlinking());

    ui::TestInput::setKeyboardFocus(surface, true);
    surface.grab();
    EXPECT_TRUE(terminal->isCursorBlinking());

    // Without a frame drawn, the next blink stops the timer.
    ui::TestInput::setKeyboardFocus(surface, false);
    EXPECT_TRUE(waitFor([&] { return !terminal->isCursorBlinking(); }));
}

TEST_F(WindowTest, CursorWithoutBlinkingNeverStartsTheTimer)
{
    m_settings.setBlinkingCursor(false);
    Windows windows(m_settings);
    TerminalWindow* window = windows.create(shell());
    ui::Window& surface = window->window();
    term::TerminalView* terminal = window->currentPage()->terminal();

    ui::TestInput::setKeyboardFocus(surface, true);
    surface.grab();
    EXPECT_FALSE(terminal->isCursorBlinking());

    ui::TestInput::setKeyboardFocus(surface, false);
    surface.grab();
    EXPECT_FALSE(terminal->isCursorBlinking());
}

// Links are underlined under the pointer only while Control is held.
TEST_F(WindowTest, LinksShowWithControl)
{
    Windows windows(m_settings);
    TerminalWindow::Options options = shell();
    options.command = {"/bin/sleep", "30"};
    TerminalWindow* window = windows.create(std::move(options));
    ui::Window& surface = window->window();
    TerminalPage* page = window->currentPage();
    term::TerminalView* view = page->terminal();
    ui::TestInput::setKeyboardFocus(surface, true);
    page->session().terminal().write("\033[?25lsee https://example.com/x here");
    page->session().outputReceived();
    base::EventLoop::current()->dispatch();
    surface.grab();
    const ui::Color mark = ui::Color::rgb(0xff00ff);
    view->setLinkColor(mark);

    const auto marked = [&] {
        base::EventLoop::current()->dispatch();
        const ui::Image frame = surface.grab();
        int count = 0;
        for (int y = 0; y < frame.height(); ++y) {
            for (int x = 0; x < frame.width(); ++x)
                count += frame.pixel(x, y) == mark;
        }
        return count;
    };
    const term::CellMetrics& cell = view->cellMetrics();
    const double scale = surface.scale();
    const ui::PointF over = view->mapToWindow({(1 + 8.5 * cell.width) / scale, (1 + 0.5 * cell.height) / scale});
    // Entering, then moving over the link.
    ui::TestInput::movePointer(surface, {over.x - 1, over.y});
    ui::TestInput::movePointer(surface, over);
    EXPECT_EQ(marked(), 0);

    ui::KeyEvent control;
    control.key = XKB_KEY_Control_L;
    ui::TestInput::pressKey(surface, control);
    EXPECT_GT(marked(), 0);
    ui::TestInput::releaseKey(surface, control);
    EXPECT_EQ(marked(), 0);
}

TEST_F(WindowTest, PointerShowsWhetherTheProgramTakesTheMouse)
{
    Windows windows(m_settings);
    TerminalWindow::Options options = shell();
    options.command = {"/bin/sleep", "30"};
    TerminalWindow* window = windows.create(std::move(options));
    TerminalPage* page = window->currentPage();
    window->window().grab();
    EXPECT_EQ(page->terminal()->cursorShape(), ui::CursorShape::Text);

    page->session().terminal().write("\033[?1000h");
    page->session().outputReceived();
    EXPECT_TRUE(waitFor([&] { return page->terminal()->cursorShape() == ui::CursorShape::Default; }));
    page->session().terminal().write("\033[?1000l");
    page->session().outputReceived();
    EXPECT_TRUE(waitFor([&] { return page->terminal()->cursorShape() == ui::CursorShape::Text; }));
}

TEST_F(WindowTest, ATinyViewLeavesTheTerminalAsItIs)
{
    Windows windows(m_settings);
    TerminalWindow::Options options = shell();
    options.command = {"/bin/sleep", "30"};
    TerminalWindow* window = windows.create(std::move(options));
    TerminalPage* page = window->currentPage();
    window->window().grab();
    term::Terminal& terminal = page->session().terminal();
    const int columns = terminal.columns();
    ASSERT_GT(columns, 20);

    terminal.write("a line long enough to wrap in a narrow terminal");
    const ui::RectF geometry = page->terminal()->geometry();
    page->terminal()->setGeometry({geometry.x, geometry.y, 1, 1});
    window->window().grab();
    EXPECT_EQ(terminal.columns(), columns);
    EXPECT_EQ(terminal.logicalLine(0).cells.back().first, 0);
}

// What a TUI frame looks like: the lines of one cell run on into the next,
// with no row of pixels between them, at a scale that is not whole.
TEST_F(WindowTest, BoxLinesJoinAcrossCellsAtFractionalScale)
{
    Windows windows(m_settings);
    TerminalWindow::Options options = shell();
    options.command = {"/bin/sleep", "30"};
    TerminalWindow* window = windows.create(std::move(options));
    TerminalPage* page = window->currentPage();
    ASSERT_NE(page, nullptr);
    page->session().terminal().write("\033[?25l\033[2J\033[H"
                                     "\xe2\x94\x8c\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x90\r\n"
                                     "\xe2\x94\x82   \xe2\x94\x82\r\n"
                                     "\xe2\x94\x82   \xe2\x94\x82\r\n"
                                     "\xe2\x94\x94\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x98");
    window->window().setScale(1.75);
    page->session().outputReceived();
    window->window().grab();
    base::EventLoop::current()->dispatch();
    const ui::Image frame = window->window().grab();

    // The scheme's foreground, which only the frame's lines are drawn in.
    const ui::Color line = ui::Color::rgb(0xc0caf5);
    term::TerminalView* view = page->terminal();
    const ui::PointF origin = view->mapToWindow({0, 0});
    const int x0 = int(std::lround(origin.x * 1.75));
    const int y0 = int(std::lround(origin.y * 1.75));
    const int x1 = std::min(frame.width(), int(std::lround((origin.x + view->geometry().width) * 1.75)));
    const int y1 = std::min(frame.height(), int(std::lround((origin.y + view->geometry().height) * 1.75)));
    int left = x1;
    int right = -1;
    int top = y1;
    int bottom = -1;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            if (frame.pixel(x, y) == line) {
                left = std::min(left, x);
                right = std::max(right, x);
                top = std::min(top, y);
                bottom = std::max(bottom, y);
            }
        }
    }
    ASSERT_LT(left, right);
    ASSERT_LT(top, bottom);
    // Each side of the frame is unbroken from corner to corner.
    for (int y = top; y <= bottom; ++y) {
        EXPECT_EQ(frame.pixel(left, y), line) << "left side, y " << y;
        EXPECT_EQ(frame.pixel(right, y), line) << "right side, y " << y;
    }
    for (int x = left; x <= right; ++x) {
        EXPECT_EQ(frame.pixel(x, top), line) << "top side, x " << x;
        EXPECT_EQ(frame.pixel(x, bottom), line) << "bottom side, x " << x;
    }

    // Light lines are two device pixels at 1.75, the frame five cells by four.
    const term::CellMetrics& cell = view->cellMetrics();
    EXPECT_EQ(right - left + 1, 4 * cell.width + 2);
    EXPECT_EQ(bottom - top + 1, 3 * cell.height + 2);
    std::printf("view at device %d,%d; cell %dx%d, baseline %d; %d columns, %d lines; right gap %d, bottom gap %d px\n",
                x0, y0, cell.width, cell.height, cell.baseline, view->columns(), view->lines(),
                x1 - x0 - 2 * 2 - view->columns() * cell.width, y1 - y0 - 2 * 2 - view->lines() * cell.height);
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    ui::Application application(argc, argv, ui::Application::Platform::Headless);
    // Character widths follow the locale, as in the application.
    std::setlocale(LC_ALL, "");
    return RUN_ALL_TESTS();
}
