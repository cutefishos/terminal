// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include "ui/application.h"
#include "ui/keys.h"
#include "ui/view.h"

#include "TerminalSession.h"

#include "scenario.h"

using namespace cutefish;

TEST(TabsTest, ManyTabsOpenAndClose)
{
    Scenario scenario;
    ui::TestDriver& driver = scenario.open(scenario.shell());
    for (int i = 0; i < 50; ++i) {
        driver.chooseMenuItem("Shell", "New Tab");
        // Until its shell has the terminal, closing the tab would ask first.
        ASSERT_TRUE(waitFor([&] { return !shownTab(driver)->session().hasActiveProcess(); })) << i;
        driver.chooseMenuItem("Shell", "Close Tab");
    }
    EXPECT_EQ(tabs(driver).size(), 1u);
}

TEST(TabsTest, ShellExitClosesItsTab)
{
    Scenario scenario;
    ui::TestDriver& driver = scenario.open(scenario.shell());
    driver.chooseMenuItem("Shell", "New Tab");
    ASSERT_EQ(tabs(driver).size(), 2u);
    run(driver, "exit 0");
    EXPECT_TRUE(waitFor([&] { return tabs(driver).size() == 1; }));
}

TEST(TabsTest, CloseOtherTabsKeepsTheChosenTab)
{
    Scenario scenario;
    ui::TestDriver& driver = scenario.open(scenario.shell());
    TerminalPage* busy = shownTab(driver);
    ASSERT_NE(busy, nullptr);
    run(driver, "sleep 30");
    ASSERT_TRUE(waitFor([&] { return busy->session().hasActiveProcess(); }));
    driver.chooseMenuItem("Shell", "New Tab");
    ASSERT_EQ(tabs(driver).size(), 2u);

    driver.chooseMenuItem("Shell", "Close Other Tabs");
    scenario.confirm(driver);
    ASSERT_EQ(tabs(driver).size(), 1u);
    ASSERT_NE(shownTab(driver), nullptr);
    EXPECT_NE(shownTab(driver), busy);
    EXPECT_TRUE(driver.shows("tab:page"));
}

TEST(TabsTest, CloseOtherTabsAfterTheKeptTabEnded)
{
    Scenario scenario;
    ui::TestDriver& driver = scenario.open(scenario.shell());
    TerminalPage* busy = shownTab(driver);
    ASSERT_NE(busy, nullptr);
    run(driver, "sleep 30");
    ASSERT_TRUE(waitFor([&] { return busy->session().hasActiveProcess(); }));
    driver.chooseMenuItem("Shell", "New Tab");
    ASSERT_EQ(tabs(driver).size(), 2u);
    run(driver, "sleep 1; exit");

    driver.chooseMenuItem("Shell", "Close Other Tabs");
    ASSERT_TRUE(waitFor([&] { return tabs(driver).size() == 1; }));
    scenario.confirm(driver);
    ASSERT_EQ(tabs(driver).size(), 1u);
    EXPECT_EQ(shownTab(driver), busy);
    EXPECT_TRUE(busy->isVisible());
}

TEST(TabsTest, LanguageChangeKeepsTheShellsAndTheSearch)
{
    Scenario scenario;
    ui::TestDriver& driver = scenario.open(scenario.shell());
    TerminalPage* busy = shownTab(driver);
    ASSERT_NE(busy, nullptr);
    run(driver, "sleep 30");
    ASSERT_TRUE(waitFor([&] { return busy->session().hasActiveProcess(); }));
    driver.chooseMenuItem("Shell", "New Tab");
    ASSERT_EQ(tabs(driver).size(), 2u);
    TerminalPage* page = shownTab(driver);
    ASSERT_NE(page, busy);
    Konsole::TerminalSession* session = &page->session();
    Konsole::TerminalView* terminal = page->terminal();

    driver.chooseMenuItem("Shell", "Rename Tab…");
    driver.typeText("Build");
    driver.typeKey(ui::key::Return);
    EXPECT_EQ(driver.text("header:title"), "Build");
    driver.chooseMenuItem("Edit", "Find…");
    EXPECT_TRUE(driver.hasFocus("find:field"));
    driver.typeText("needle");

    driver.chooseMenuItem("Terminal", "Settings…");
    ui::TestDriver& settings = scenario.dialog(driver);
    ASSERT_NE(&settings.window(), &driver.window());
    const ui::View* settingsContent = settings.find("settings:content");
    ASSERT_NE(settingsContent, nullptr);

    driver.chooseMenuItem("Shell", "Close Other Tabs");

    ui::Application::instance()->languageChanged.emit();
    ASSERT_EQ(tabs(driver).size(), 2u);
    EXPECT_EQ(shownTab(driver), page);
    EXPECT_EQ(&page->session(), session);
    EXPECT_EQ(page->terminal(), terminal);
    EXPECT_EQ(page->title(), "Build");
    EXPECT_TRUE(busy->session().hasActiveProcess());
    EXPECT_EQ(driver.text("find:field"), "needle");
    const ui::View* rebuilt = settings.find("settings:content");
    ASSERT_NE(rebuilt, nullptr);
    EXPECT_NE(rebuilt, settingsContent);

    // The close question is asked again and still closes the other tab.
    scenario.confirm(driver);
    ASSERT_EQ(tabs(driver).size(), 1u);
    EXPECT_EQ(shownTab(driver), page);
}
