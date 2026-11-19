// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <vector>

#include "ui/application.h"
#include "ui/menu.h"

#include "scenario.h"

using namespace cutefish;

TEST(MenuTest, EachWindowShowsItsMenusInTheMenuBar)
{
    Scenario scenario;
    ui::TestDriver& first = scenario.open(scenario.shell());
    ui::TestDriver& second = scenario.open(scenario.shell());
    for (ui::TestDriver* driver : {&first, &second}) {
        ui::Menu* menu = driver->window().mainMenu();
        ASSERT_TRUE(menu);
        const std::vector<ui::MenuRole> roles{ui::MenuRole::Application, ui::MenuRole::None, ui::MenuRole::None,
                                              ui::MenuRole::None, ui::MenuRole::Windows};
        for (size_t i = 0; i < roles.size(); ++i) {
            ASSERT_TRUE(menu->submenu(i)) << i;
            EXPECT_EQ(menu->submenu(i)->role(), roles[i]) << i;
        }
        EXPECT_FALSE(menu->submenu(roles.size()));
    }
    EXPECT_NE(first.window().mainMenu(), second.window().mainMenu());

    ui::Application::instance()->languageChanged.emit();
    ASSERT_TRUE(first.window().mainMenu());
    EXPECT_EQ(first.window().mainMenu()->submenu(0)->role(), ui::MenuRole::Application);
}
