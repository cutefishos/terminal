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

TEST(InteractionTest, ShiftClickExtendsTheSelection)
{
    Scenario scenario;
    ui::TestDriver& driver = scenario.open(printer(scenario, kText));
    ASSERT_TRUE(waitFor([&] { return shows(driver, "charlie"); }));
    clickAt(driver, cellCentre(driver, cellOf(driver, "ravo")), 2);
    ASSERT_EQ(selection(driver), "bravo");

    // By words, as the double click selected.
    const ui::PointF end = cellCentre(driver, cellOf(driver, "arlie"));
    ui::TestInput::movePointer(driver.window(), end);
    ui::TestInput::press(driver.window(), end, ui::MouseButton::Left, ui::ShiftModifier);
    ui::TestInput::release(driver.window(), end);
    EXPECT_EQ(selection(driver), "bravo charlie");
    EXPECT_EQ(primarySelection(), "bravo charlie");
}

TEST(InteractionTest, AProgramTakingTheMouseGetsClicksAndShiftedWheelsAndClearsTheSelection)
{
    Scenario scenario;
    const std::string path = scenario.directory() + "/received";
    ui::TestDriver& driver = scenario.open(scenario.recorder(path, "alpha bravo\\r\\n\\033[?1000h\\033[?1006h"));
    ASSERT_TRUE(waitFor([&] { return takesTheMouse(driver); }));

    // Shift keeps clicks for selecting.
    const ui::PointF word = cellCentre(driver, cellOf(driver, "ravo"));
    ui::TestInput::movePointer(driver.window(), word);
    for (int i = 0; i < 2; ++i) {
        ui::TestInput::press(driver.window(), word, ui::MouseButton::Left, ui::ShiftModifier);
        ui::TestInput::release(driver.window(), word);
    }
    ASSERT_EQ(selection(driver), "bravo");

    ui::TestInput::click(driver.window(), cellCentre(driver, cellOf(driver, "lpha")));
    EXPECT_TRUE(waitFor([&] { return contents(path).find("\033[<0;") != std::string::npos; })) << contents(path);
    EXPECT_EQ(selection(driver), "");

    // The wheel goes to the program with Shift as well.
    ui::ScrollEvent wheel;
    wheel.value120Y = -120;
    wheel.modifiers = ui::ShiftModifier;
    ui::TestInput::scroll(driver.window(), word, wheel);
    EXPECT_TRUE(waitFor([&] { return contents(path).find("\033[<68;") != std::string::npos; })) << contents(path);
}
