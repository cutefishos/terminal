// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
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

TEST(InteractionTest, WheelOnTheAlternateScreenSendsArrowsOnlyInAlternateScrollMode)
{
    Scenario scenario;
    const std::string path = scenario.directory() + "/received";
    ui::TestDriver& driver = scenario.open(scenario.recorder(path, "\\033[?1049h\\033[?1007l"));
    ASSERT_TRUE(waitFor([&] {
        TerminalPage* page = shownTab(driver);
        return page && page->session().terminal().usesAlternateScreen()
                && !page->session().terminal().mode(GHOSTTY_MODE_MOUSE_ALTERNATE_SCROLL);
    }));
    ui::ScrollEvent wheel;
    wheel.value120Y = -120;
    driver.scroll("tab:page", wheel);
    driver.typeText("x");
    EXPECT_TRUE(waitFor([&] { return contents(path) == "x"; })) << contents(path);
}

TEST(InteractionTest, OnlyAPasteThatCouldRunCommandsAsksFirst)
{
    Scenario scenario;
    const std::string bracketed = scenario.directory() + "/bracketed";
    ui::TestDriver& driver = scenario.open(scenario.recorder(bracketed));
    ASSERT_TRUE(waitFor([&] { return shownTab(driver)->session().terminal().bracketedPaste(); }));
    // Bracketed, the lines arrive as one paste the program handles.
    ui::Clipboard::setText("one\ntwo");
    driver.chooseMenuItem("Edit", "Paste");
    EXPECT_EQ(driver.dialog(), nullptr);
    EXPECT_TRUE(waitFor([&] { return contents(bracketed) == "\033[200~one\ntwo\033[201~"; })) << contents(bracketed);

    const std::string plain = scenario.directory() + "/plain";
    ui::TestDriver& other = scenario.open(scenario.recorder(plain, "ready"));
    ASSERT_TRUE(waitFor([&] { return shows(other, "ready"); }));
    other.chooseMenuItem("Edit", "Paste");
    EXPECT_NE(other.dialog(), nullptr);
}

TEST(InteractionTest, FocusReportsFollowTheWindowsKeyboard)
{
    Scenario scenario;
    const std::string path = scenario.directory() + "/received";
    ui::TestDriver& driver = scenario.open(scenario.recorder(path, "\\033[?1004h"));
    ASSERT_TRUE(waitFor([&] { return shownTab(driver)->session().terminal().mode(GHOSTTY_MODE_FOCUS_EVENT); }));
    // The window draws a frame for each change, as on screen.
    driver.window().grab();
    const std::string before = contents(path);
    driver.setKeyboardFocus(false);
    driver.window().grab();
    EXPECT_TRUE(waitFor([&] { return contents(path) == before + "\033[O"; })) << contents(path);
    driver.setKeyboardFocus(true);
    driver.window().grab();
    EXPECT_TRUE(waitFor([&] { return contents(path) == before + "\033[O\033[I"; })) << contents(path);
}

TEST(InteractionTest, ControlClickOnALinkGoesToAProgramTakingTheMouse)
{
    Scenario scenario;
    const std::string path = scenario.directory() + "/received";
    ui::TestDriver& driver =
            scenario.open(scenario.recorder(path, "ftp://example.com/x\\r\\n\\033[?1000h\\033[?1006h"));
    ASSERT_TRUE(waitFor([&] { return takesTheMouse(driver); }));
    const ui::PointF link = cellCentre(driver, cellOf(driver, "example"));
    ui::TestInput::movePointer(driver.window(), link);
    ui::TestInput::press(driver.window(), link, ui::MouseButton::Left, ui::ControlModifier);
    ui::TestInput::release(driver.window(), link);
    EXPECT_TRUE(waitFor([&] { return contents(path).find("\033[<16;") != std::string::npos; })) << contents(path);
    EXPECT_EQ(driver.dialog(), nullptr);
}

TEST(InteractionTest, AKeyHeldAsTheKeyboardLeavesIsReleasedForTheProgram)
{
    Scenario scenario;
    const std::string path = scenario.directory() + "/received";
    // Every key as an escape code, with its release.
    ui::TestDriver& driver = scenario.open(scenario.recorder(path, "\\033[>11u"));
    ASSERT_TRUE(waitFor([&] {
        uint8_t flags = 0;
        ghostty_terminal_get(shownTab(driver)->session().terminal().handle(),
                             GHOSTTY_TERMINAL_DATA_KITTY_KEYBOARD_FLAGS, &flags);
        return flags == 11;
    }));
    driver.window().grab();
    ui::KeyEvent key;
    key.key = 'a';
    key.text = "A";
    key.modifiers = ui::ShiftModifier;
    driver.pressKey(key);
    ASSERT_TRUE(waitFor([&] { return !contents(path).empty(); }));
    const std::string pressed = contents(path);
    driver.setKeyboardFocus(false);
    driver.window().grab();
    // The key, then Shift on both sides.
    EXPECT_TRUE(waitFor([&] {
        return contents(path) == pressed + "\033[97;2:3u\033[57441;1:3u\033[57447;1:3u";
    })) << contents(path);
}

TEST(InteractionTest, APathLinkOpensFromTheShellsDirectory)
{
    Scenario scenario;
    std::filesystem::create_directories(scenario.directory() + "/notes.d");
    ASSERT_TRUE(cutefish::base::file::writeAtomically(scenario.directory() + "/notes.d/today.txt", "x"));
    ui::TestDriver& driver = scenario.open(printer(scenario, "see notes.d/today.txt and /nowhere/else.txt\\r\\n"));
    ASSERT_TRUE(waitFor([&] { return shows(driver, "else.txt"); }));
    term::TerminalView* view = shownTab(driver)->terminal();
    const auto pointAt = [&](const char* text) {
        const ui::PointF window = cellCentre(driver, cellOf(driver, text));
        const ui::PointF origin = view->mapToWindow({0, 0});
        return ui::PointF{window.x - origin.x, window.y - origin.y};
    };
    // Relative and there: from the directory. Not there: as written.
    ui::PointF relative = pointAt("today");
    EXPECT_EQ(view->linkAt(relative.x, relative.y), scenario.directory() + "/notes.d/today.txt");
    ui::PointF absolute = pointAt("else");
    EXPECT_EQ(view->linkAt(absolute.x, absolute.y), "/nowhere/else.txt");
}

TEST(InteractionTest, ControlShiftUpAndDownGoFromPromptToPrompt)
{
    Scenario scenario;
    std::string text;
    for (int command = 0; command < 4; ++command) {
        text += "\\033]133;A\\a$ \\033]133;B\\acommand" + std::to_string(command) + "\\r\\n\\033]133;C\\a";
        for (int line = 0; line < 40; ++line)
            text += "output " + std::to_string(command) + "." + std::to_string(line) + "\\r\\n";
    }
    ui::TestDriver& driver = scenario.open(printer(scenario, text));
    ASSERT_TRUE(waitFor([&] { return shows(driver, "output 3.39"); }));
    term::TerminalView* view = shownTab(driver)->terminal();
    term::Terminal& terminal = shownTab(driver)->session().terminal();
    const auto topLine = [&] { return terminal.logicalLine(0).text; };

    driver.typeKey(ui::key::Up, ui::ControlModifier | ui::ShiftModifier);
    EXPECT_EQ(topLine().substr(0, 10), "$ command3");
    driver.typeKey(ui::key::Up, ui::ControlModifier | ui::ShiftModifier);
    EXPECT_EQ(topLine().substr(0, 10), "$ command2");
    driver.typeKey(ui::key::Down, ui::ControlModifier | ui::ShiftModifier);
    EXPECT_EQ(topLine().substr(0, 10), "$ command3");
    EXPECT_LT(view->scrollbarValue(), view->scrollbarMaximum());
}

TEST(InteractionTest, ControlTripleClickSelectsACommandsOutput)
{
    Scenario scenario;
    std::string text;
    for (int command = 0; command < 3; ++command) {
        text += "\\033]133;A\\a$ \\033]133;B\\acommand" + std::to_string(command) + "\\r\\n\\033]133;C\\a";
        for (int line = 0; line < 3; ++line)
            text += "output " + std::to_string(command) + "." + std::to_string(line) + "\\r\\n";
    }
    ui::TestDriver& driver = scenario.open(printer(scenario, text));
    ASSERT_TRUE(waitFor([&] { return shows(driver, "output 2.2"); }));
    const ui::PointF point = cellCentre(driver, cellOf(driver, "output 1.1"));
    ui::TestInput::movePointer(driver.window(), point);
    for (int i = 0; i < 3; ++i) {
        ui::TestInput::press(driver.window(), point, ui::MouseButton::Left, ui::ControlModifier);
        ui::TestInput::release(driver.window(), point);
    }
    EXPECT_EQ(selection(driver), "output 1.0\noutput 1.1\noutput 1.2");
}

TEST(InteractionTest, AClickOnTheCommandLineMovesBashsCursor)
{
    Scenario scenario;
    // bash with the integration, reading the scenario's own startup files.
    const char* home = std::getenv("HOME");
    const std::string savedHome = home ? home : "";
    ::setenv("HOME", scenario.directory().c_str(), 1);
    ASSERT_TRUE(cutefish::base::file::writeAtomically(scenario.directory() + "/.bashrc", "PS1='$ '\n"));
    term::Session::setShellIntegrationDirectory(TERMINAL_SOURCE_DIR "/shell-integration");
    TerminalWindow::Options options = scenario.shell();
    options.command = {"/usr/bin/bash"};
    struct RestoreHome
    {
        std::string home;
        ~RestoreHome() { ::setenv("HOME", home.c_str(), 1); }
    } restore{savedHome};
    ui::TestDriver& driver = scenario.open(std::move(options));
    term::Terminal& terminal = shownTab(driver)->session().terminal();
    ASSERT_TRUE(waitFor([&] { return terminal.cursorAtPrompt() && shows(driver, "$ "); }));

    driver.typeText("echo hello");
    ASSERT_TRUE(waitFor([&] { return shows(driver, "$ echo hello"); }));
    ui::TestInput::click(driver.window(), cellCentre(driver, cellOf(driver, "hello")));
    driver.typeText("X");
    driver.typeKey(ui::key::Return, ui::NoModifier, "\r");
    EXPECT_TRUE(waitFor([&] { return shows(driver, "Xhello"); }));
    EXPECT_FALSE(shows(driver, "helloX"));
}
