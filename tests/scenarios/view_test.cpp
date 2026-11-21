// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <utility>

#include "ui/clipboard.h"
#include "ui/event.h"
#include "ui/keys.h"
#include "ui/test_input.h"

#include "session.h"
#include "terminalview.h"

#include "scenario.h"

using namespace cutefish;

// A program that prints the text and then sits there, so nothing else lands on the screen.
static TerminalWindow::Options printer(const Scenario& scenario, const std::string& text)
{
    TerminalWindow::Options options = scenario.shell();
    options.command = {"/bin/sh", "-c", "printf '" + text + "'; exec sleep 30"};
    return options;
}

static bool shows(ui::TestDriver& driver, const std::string& text)
{
    TerminalPage* page = shownTab(driver);
    if (!page)
        return false;
    term::Terminal& terminal = page->session().terminal();
    for (int line = 0; line < terminal.lines(); ++line) {
        if (terminal.logicalLine(line).text.find(text) != std::string::npos)
            return true;
    }
    return false;
}

static std::string selection(ui::TestDriver& driver)
{
    return shownTab(driver)->terminal()->selectedText();
}

// The viewport cell where the text starts, in (column, line); (-1, -1) when
// it shows nowhere.
static std::pair<int, int> cellOf(ui::TestDriver& driver, const std::string& text)
{
    term::Terminal& terminal = shownTab(driver)->session().terminal();
    for (int line = 0; line < terminal.lines(); ++line) {
        const term::Terminal::LogicalLine logical = terminal.logicalLine(line);
        const size_t at = logical.text.find(text);
        if (at != std::string::npos)
            return {logical.cells[at].second, logical.cells[at].first};
    }
    return {-1, -1};
}

// The middle of a viewport cell, in the window's coordinates.
static ui::PointF cellCentre(ui::TestDriver& driver, std::pair<int, int> cell)
{
    term::TerminalView* view = shownTab(driver)->terminal();
    const term::CellMetrics& metrics = view->cellMetrics();
    const double scale = driver.window().scale();
    const double margin = std::lround(scale);
    return view->mapToWindow({(margin + (cell.first + 0.5) * metrics.width) / scale,
                              (margin + (cell.second + 0.5) * metrics.height) / scale});
}

// Pressed as many times as the count, released but for the last when held.
static void clickAt(ui::TestDriver& driver, ui::PointF point, int count, bool hold = false)
{
    ui::TestInput::movePointer(driver.window(), point);
    for (int i = 0; i < count; ++i) {
        ui::TestInput::press(driver.window(), point);
        if (!hold || i + 1 < count)
            ui::TestInput::release(driver.window(), point);
    }
}

static std::string primarySelection()
{
    std::string text;
    ui::Clipboard::requestText([&](std::string received) { text = std::move(received); }, ui::ClipboardMode::Selection);
    return text;
}

static std::string copied(ui::TestDriver& driver)
{
    driver.chooseMenuItem("Edit", "Copy");
    std::string text;
    ui::Clipboard::requestText([&](std::string received) { text = std::move(received); });
    return text;
}

TEST(ViewTest, FindGoesFromNewestToOlderWithSmartCase)
{
    Scenario scenario;
    ui::TestDriver& driver = scenario.open(printer(scenario, "Needle one\\r\\nneedle two\\r\\nNEEDLE three\\r\\n"));
    ASSERT_TRUE(waitFor([&] { return shows(driver, "NEEDLE three"); }));

    driver.chooseMenuItem("Edit", "Find…");
    ASSERT_TRUE(driver.hasFocus("find:field"));
    driver.typeText("needle");
    EXPECT_EQ(selection(driver), "NEEDLE");
    driver.typeKey(ui::key::Return);
    EXPECT_EQ(selection(driver), "needle");
    driver.typeKey(ui::key::Return, ui::ShiftModifier);
    EXPECT_EQ(selection(driver), "NEEDLE");

    // A capital letter makes the search match case.
    driver.typeKey(ui::key::Escape);
    driver.chooseMenuItem("Edit", "Find…");
    driver.typeText("Needle");
    EXPECT_EQ(selection(driver), "Needle");
    driver.typeKey(ui::key::Return);
    EXPECT_EQ(selection(driver), "Needle");
}

TEST(ViewTest, SelectAllCopiesTheText)
{
    Scenario scenario;
    ui::TestDriver& driver = scenario.open(printer(scenario, "first line\\r\\nsecond line\\r\\n"));
    ASSERT_TRUE(waitFor([&] { return shows(driver, "second line"); }));
    driver.chooseMenuItem("Edit", "Select All");
    driver.chooseMenuItem("Edit", "Copy");
    std::string copied;
    ui::Clipboard::requestText([&](std::string text) { copied = std::move(text); });
    EXPECT_EQ(copied, "first line\nsecond line");
}

TEST(ViewTest, WheelAndShiftPageUpScrollTheHistory)
{
    Scenario scenario;
    std::string lines;
    for (int i = 0; i < 200; ++i)
        lines += "line " + std::to_string(i) + "\\r\\n";
    ui::TestDriver& driver = scenario.open(printer(scenario, lines));
    ASSERT_TRUE(waitFor([&] { return shows(driver, "line 199"); }));
    term::TerminalView* view = shownTab(driver)->terminal();
    ASSERT_TRUE(waitFor([&] { return view->scrollbarMaximum() > 100; }));
    const int bottom = view->scrollbarMaximum();
    EXPECT_EQ(view->scrollbarValue(), bottom);

    ui::ScrollEvent wheel;
    wheel.value120Y = -120;
    driver.scroll("tab:page", wheel);
    EXPECT_EQ(view->scrollbarValue(), bottom - 3);

    driver.typeKey(ui::key::PageUp, ui::ShiftModifier);
    EXPECT_EQ(view->scrollbarValue(), bottom - 3 - (view->lines() - 1));
    driver.typeKey(ui::key::End, ui::ShiftModifier);
    EXPECT_EQ(view->scrollbarValue(), bottom);
}

TEST(ViewTest, ShiftEnterReachesAProgramThatAsksForKeys)
{
    Scenario scenario;
    const std::string path = scenario.directory() + "/received";
    ui::TestDriver& driver = scenario.open(scenario.recorder(path, "\\033[>1u"));
    ASSERT_TRUE(waitFor([&] {
        TerminalPage* page = shownTab(driver);
        return page && page->session().foregroundProcessName() == "cat";
    }));
    ASSERT_TRUE(waitFor([&] {
        uint8_t flags = 0;
        ghostty_terminal_get(shownTab(driver)->session().terminal().handle(), GHOSTTY_TERMINAL_DATA_KITTY_KEYBOARD_FLAGS,
                             &flags);
        return flags == 1;
    }));
    driver.typeKey(ui::key::Return, ui::ShiftModifier, "\r");
    driver.typeText("x");
    EXPECT_TRUE(waitFor([&] { return contents(path) == "\033[13;2ux"; })) << contents(path);
}

static const char kWords[] = "alpha bravo charlie\\r\\n"
                             "see /usr/local/share/doc/file.txt here\\r\\n"
                             "open https://example.com/path?q=1 now\\r\\n";

// Word characters are the library's: a path is one word, and a colon ends
// one, so a URL's scheme and the rest are two.
TEST(ViewTest, DoubleClickSelectsAWord)
{
    Scenario scenario;
    ui::TestDriver& driver = scenario.open(printer(scenario, kWords));
    ASSERT_TRUE(waitFor([&] { return shows(driver, "open https"); }));

    const std::pair<std::string, std::string> cases[] = {
        {"ravo", "bravo"},
        {"local", "/usr/local/share/doc/file.txt"},
        {"file.txt", "/usr/local/share/doc/file.txt"},
        {"ttps", "https"},
        {"ample.com", "//example.com/path?q=1"},
    };
    for (const auto& [at, word] : cases) {
        SCOPED_TRACE(at);
        clickAt(driver, cellCentre(driver, cellOf(driver, at)), 2);
        EXPECT_EQ(selection(driver), word);
        EXPECT_EQ(primarySelection(), word);
        EXPECT_EQ(copied(driver), word);
    }
}

TEST(ViewTest, TripleClickSelectsTheWholeWrappedLine)
{
    Scenario scenario;
    std::string line = "first";
    for (int i = 0; i < 60; ++i)
        line += " word" + std::to_string(i);
    line += " last";
    ui::TestDriver& driver = scenario.open(printer(scenario, "above\\r\\n" + line + "\\r\\nbelow\\r\\n"));
    ASSERT_TRUE(waitFor([&] { return shows(driver, "below"); }));
    // On a row the line wraps onto, neither its first nor its last.
    const std::pair<int, int> first = cellOf(driver, "first");
    const std::pair<int, int> last = cellOf(driver, "last");
    ASSERT_GE(last.second - first.second, 2);
    clickAt(driver, cellCentre(driver, {3, first.second + 1}), 3);
    EXPECT_EQ(selection(driver), line);
    EXPECT_EQ(primarySelection(), line);
    EXPECT_EQ(copied(driver), line);
}

TEST(ViewTest, DragAfterADoubleClickExtendsByWords)
{
    Scenario scenario;
    ui::TestDriver& driver = scenario.open(printer(scenario, kWords));
    ASSERT_TRUE(waitFor([&] { return shows(driver, "open https"); }));
    clickAt(driver, cellCentre(driver, cellOf(driver, "lpha")), 2, true);
    // Into the middle of a word, which comes whole.
    ui::TestInput::movePointer(driver.window(), cellCentre(driver, cellOf(driver, "arlie")));
    ui::TestInput::release(driver.window(), cellCentre(driver, cellOf(driver, "arlie")));
    EXPECT_EQ(selection(driver), "alpha bravo charlie");
    EXPECT_EQ(primarySelection(), "alpha bravo charlie");
    EXPECT_EQ(copied(driver), "alpha bravo charlie");
}

TEST(ViewTest, DragAfterATripleClickExtendsByLines)
{
    Scenario scenario;
    ui::TestDriver& driver = scenario.open(printer(scenario, kWords));
    ASSERT_TRUE(waitFor([&] { return shows(driver, "open https"); }));
    clickAt(driver, cellCentre(driver, cellOf(driver, "ravo")), 3, true);
    // The first column of the next line takes that line whole.
    const ui::PointF next = cellCentre(driver, {0, cellOf(driver, "see").second});
    ui::TestInput::movePointer(driver.window(), next);
    ui::TestInput::release(driver.window(), next);
    const std::string expected = "alpha bravo charlie\nsee /usr/local/share/doc/file.txt here";
    EXPECT_EQ(selection(driver), expected);
    EXPECT_EQ(primarySelection(), expected);
    EXPECT_EQ(copied(driver), expected);
}
