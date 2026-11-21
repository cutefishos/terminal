// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <string>

#include <xkbcommon/xkbcommon-keysyms.h>

#include "ui/clipboard.h"
#include "ui/event.h"
#include "ui/keys.h"
#include "ui/test_input.h"

#include "session.h"
#include "terminalview.h"

#include "scenario.h"

using namespace cutefish;

static const char kText[] = "alpha bravo charlie\\r\\n";

static bool takesTheMouse(ui::TestDriver& driver)
{
    TerminalPage* page = shownTab(driver);
    return page && page->session().terminal().tracksMouse();
}

TEST(InteractionTest, TypingClearsTheSelection)
{
    Scenario scenario;
    ui::TestDriver& driver = scenario.open(printer(scenario, kText));
    ASSERT_TRUE(waitFor([&] { return shows(driver, "charlie"); }));
    // Each on another word, so that no click counts as a third.
    clickAt(driver, cellCentre(driver, cellOf(driver, "ravo")), 2);
    ASSERT_EQ(selection(driver), "bravo");
    // A modifier alone sends nothing.
    driver.typeKey(XKB_KEY_Shift_L);
    EXPECT_EQ(selection(driver), "bravo");
    driver.typeText("x");
    EXPECT_EQ(selection(driver), "");

    clickAt(driver, cellCentre(driver, cellOf(driver, "lpha")), 2);
    ASSERT_EQ(selection(driver), "alpha");
    driver.typeKey(ui::key::Escape);
    EXPECT_EQ(selection(driver), "");

    clickAt(driver, cellCentre(driver, cellOf(driver, "arlie")), 2);
    ASSERT_EQ(selection(driver), "charlie");
    ui::TextInputEvent composing;
    composing.preedit = "y";
    ui::TestInput::inputMethod(driver.window(), composing);
    EXPECT_EQ(selection(driver), "");
}
