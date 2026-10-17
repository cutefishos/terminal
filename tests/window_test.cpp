// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include "base/event_loop.h"
#include "base/file.h"
#include "base/timer.h"
#include "ui/glyph_run.h"
#include "ui/painter.h"
#include "ui/view.h"
#include "ui/window.h"

#include "TerminalFont.h"
#include "tools.h"

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
    void SetUp() override
    {
        set_kb_layout_dir(KB_LAYOUTS_DIR);
        ASSERT_TRUE(m_directory.isValid());
    }

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

TEST_F(WindowTest, ManyTabsOpenAndClose)
{
    Windows windows(m_settings);
    TerminalWindow* window = windows.create(shell());
    for (int i = 0; i < 50; ++i) {
        window->openTab(m_directory.path(), {"/bin/sh"});
        window->closeTab(1);
    }
    EXPECT_EQ(window->tabCount(), 1);
}

TEST_F(WindowTest, ShellExitClosesItsTab)
{
    Windows windows(m_settings);
    TerminalWindow* window = windows.create(shell());
    window->openTab(m_directory.path(), {"/bin/sh", "-c", "exit 0"});
    EXPECT_TRUE(waitFor([&] { return window->tabCount() == 1; }));
}

namespace {

class Canvas : public ui::View
{
public:
    explicit Canvas(std::function<void(ui::Painter&)> draw)
        : m_draw(std::move(draw))
    {
    }

protected:
    void paint(ui::Painter& painter) override { m_draw(painter); }

private:
    std::function<void(ui::Painter&)> m_draw;
};

}

TEST(TerminalFontTest, ShapedClustersStayBounded)
{
    Konsole::TerminalFont font;
    font.setFont("Monospace", 10);

    // Distinct ideographs, a hundred a call, as output of every code point would draw them.
    auto draw = [&font](ui::Painter& painter) {
        char32_t character = 0x4E00;
        std::vector<std::u32string> texts;
        for (int call = 0; call < 200; ++call) {
            texts.clear();
            for (int i = 0; i < 100; ++i)
                texts.emplace_back(1, character++);
            std::vector<ui::GlyphCluster> clusters;
            for (const std::u32string& text : texts)
                clusters.push_back({text, 0});
            font.draw(painter, Konsole::TerminalFont::Regular, ui::Color::rgb(0), 1, 12, clusters, {1, 1}, {});
        }
    };

    ui::Window window;
    window.resize({100, 60});
    window.setContentView(std::make_unique<Canvas>(draw));
    window.show();
    window.grab();

    EXPECT_LE(font.cachedClusters(), 8192u + 100u);
}
