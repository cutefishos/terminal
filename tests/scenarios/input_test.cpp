// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <string>

#include "ui/clipboard.h"
#include "ui/event.h"

#include "session.h"

#include "scenario.h"

using namespace cutefish;

static bool asksForBracketedPaste(ui::TestDriver& driver)
{
    TerminalPage* page = shownTab(driver);
    return page && page->session().terminal().bracketedPaste();
}

TEST(InputTest, PasteCannotEndTheBracketEarly)
{
    Scenario scenario;
    const std::string path = scenario.directory() + "/received";
    ui::TestDriver& driver = scenario.open(scenario.recorder(path));
    ASSERT_TRUE(waitFor([&] { return asksForBracketedPaste(driver); }));

    ui::Clipboard::setText("echo \033[201~touch pwned");
    driver.chooseMenuItem("Edit", "Paste");
    // The ESC that would end the paste reaches the program as a space.
    const std::string expected = "\033[200~echo  [201~touch pwned\033[201~";
    EXPECT_TRUE(waitFor([&] { return contents(path) == expected; })) << contents(path);
}

TEST(InputTest, DroppedFilesArriveAsAPaste)
{
    Scenario scenario;
    const std::string path = scenario.directory() + "/received";
    ui::TestDriver& driver = scenario.open(scenario.recorder(path));
    ASSERT_TRUE(waitFor([&] { return asksForBracketedPaste(driver); }));

    // Ctrl+U in a name would erase the opening quote at a prompt; it arrives as a space.
    driver.drop("tab:page", {{"text/uri-list", "file:///tmp/a%15b%20c\r\n"}});
    const std::string expected = "\033[200~'/tmp/a b c' \033[201~";
    EXPECT_TRUE(waitFor([&] { return contents(path) == expected; })) << contents(path);
}

TEST(InputTest, DroppedFileUrlsWithAHostBecomePaths)
{
    Scenario scenario;
    const std::string path = scenario.directory() + "/received";
    ui::TestDriver& driver = scenario.open(scenario.recorder(path));
    ASSERT_TRUE(waitFor([&] { return asksForBracketedPaste(driver); }));

    driver.drop("tab:page", {{"text/uri-list",
                              "file://localhost/tmp/a%23b\r\nfile://elsewhere/tmp/c\r\nhttps://example.com/d\r\n"}});
    const std::string expected = "\033[200~'/tmp/a#b' file://elsewhere/tmp/c https://example.com/d \033[201~";
    EXPECT_TRUE(waitFor([&] { return contents(path) == expected; })) << contents(path);
}

// A wheel turn up, three lines.
static ui::ScrollEvent wheelUp()
{
    ui::ScrollEvent event;
    event.value120Y = -120;
    return event;
}

TEST(InputTest, WheelWithoutScrollbackSendsArrowsOnlyOnTheAlternateScreen)
{
    Scenario scenario;
    const std::string primary = scenario.directory() + "/primary";
    ui::TestDriver& driver = scenario.open(scenario.recorder(primary));
    ASSERT_TRUE(waitFor([&] { return asksForBracketedPaste(driver); }));
    driver.scroll("tab:page", wheelUp());
    driver.typeText("x");
    EXPECT_TRUE(waitFor([&] { return contents(primary) == "x"; })) << contents(primary);

    const std::string alternate = scenario.directory() + "/alternate";
    ui::TestDriver& other = scenario.open(scenario.recorder(alternate, "\\033[?1049h"));
    ASSERT_TRUE(waitFor([&] {
        TerminalPage* page = shownTab(other);
        return page && page->session().terminal().usesAlternateScreen();
    }));
    other.scroll("tab:page", wheelUp());
    other.typeText("x");
    EXPECT_TRUE(waitFor([&] { return contents(alternate) == "\033[A\033[A\033[Ax"; })) << contents(alternate);
}
