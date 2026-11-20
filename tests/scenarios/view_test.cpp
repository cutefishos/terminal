// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <string>

#include "ui/clipboard.h"
#include "ui/event.h"
#include "ui/keys.h"

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
