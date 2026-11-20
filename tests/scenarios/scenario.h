// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <gtest/gtest.h>

#include <chrono>
#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "base/file.h"
#include "ui/keys.h"
#include "ui/test_driver.h"
#include "ui/window.h"

#include "terminalpage.h"
#include "terminalsettings.h"
#include "terminalwindow.h"
#include "windows.h"

// Terminal windows of one process, with settings of their own, each shown
// and worked only through a driver.
class Scenario
{
public:
    Scenario()
    {
        EXPECT_TRUE(m_directory.isValid());
    }

    const std::string& directory() const { return m_directory.path(); }

    // /bin/sh in the scenario's directory.
    TerminalWindow::Options shell() const
    {
        TerminalWindow::Options options;
        options.startDirectory = directory();
        options.command = {"/bin/sh"};
        options.restoresSize = false;
        return options;
    }

    // A program that takes the keyboard raw, writes `output`, by default
    // asking for bracketed paste, and then writes what it receives to `path`.
    TerminalWindow::Options recorder(const std::string& path, const std::string& output = "\\033[?2004h") const
    {
        TerminalWindow::Options options = shell();
        options.command = {"/bin/sh", "-c", "stty raw -echo; printf '" + output + "'; exec cat > '" + path + "'"};
        return options;
    }

    // A new window of the process, shown, with the keyboard.
    cutefish::ui::TestDriver& open(TerminalWindow::Options options)
    {
        cutefish::ui::TestDriver& driver = attach(m_windows.create(std::move(options))->window());
        driver.setKeyboardFocus(true);
        return driver;
    }

    // The dialog the window shows, which must be there.
    cutefish::ui::TestDriver& dialog(cutefish::ui::TestDriver& driver)
    {
        cutefish::ui::Window* window = driver.dialog();
        EXPECT_NE(window, nullptr);
        return attach(window ? *window : driver.window());
    }

    // Answers the window's question with Return, as its default button does.
    void confirm(cutefish::ui::TestDriver& driver)
    {
        cutefish::ui::TestDriver& question = dialog(driver);
        ASSERT_NE(&question.window(), &driver.window());
        question.setKeyboardFocus(true);
        question.typeKey(cutefish::ui::key::Return);
    }

private:
    cutefish::ui::TestDriver& attach(cutefish::ui::Window& window)
    {
        m_drivers.emplace_back(window, [](const std::string& message) { ADD_FAILURE() << message; });
        m_drivers.back().layout();
        return m_drivers.back();
    }

    cutefish::base::file::TemporaryDirectory m_directory{"/tmp", "terminal-scenario"};
    TerminalSettings m_settings{m_directory.path() + "/terminal.conf"};
    Windows m_windows{m_settings};
    std::deque<cutefish::ui::TestDriver> m_drivers;
};

// The window's tabs, in no particular order.
inline std::vector<TerminalPage*> tabs(cutefish::ui::TestDriver& driver)
{
    return driver.findAll<TerminalPage>("tab:page");
}

// The tab the window shows.
inline TerminalPage* shownTab(cutefish::ui::TestDriver& driver)
{
    for (TerminalPage* page : tabs(driver)) {
        if (page->isVisible())
            return page;
    }
    return nullptr;
}

// A command line typed into the shown tab, and Enter.
inline void run(cutefish::ui::TestDriver& driver, const std::string& command)
{
    driver.typeText(command);
    driver.typeKey(cutefish::ui::key::Return, cutefish::ui::NoModifier, "\r");
}

// Shells take longer than views to answer.
inline bool waitFor(const std::function<bool()>& condition)
{
    return cutefish::ui::TestDriver::waitFor(condition, std::chrono::seconds(5));
}

inline std::string contents(const std::string& path)
{
    return cutefish::base::file::read(path).value_or(std::string());
}
