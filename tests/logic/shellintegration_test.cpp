// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unistd.h>

#include "base/event_loop.h"
#include "base/file.h"
#include "base/timer.h"

#include "session.h"
#include "shellintegration.h"

using namespace term;

static const std::string kScripts = TERMINAL_SOURCE_DIR "/shell-integration";

static Pty::Options launch(std::vector<std::string> arguments)
{
    Pty::Options options;
    options.program = arguments.front();
    options.arguments = std::move(arguments);
    return options;
}

TEST(ShellIntegrationTest, BashReadsTheScriptInPosixMode)
{
    Pty::Options options = launch({"/usr/bin/bash", "--norc", "-i"});
    ASSERT_TRUE(integrateShell(kScripts, options));
    EXPECT_EQ(options.arguments, (std::vector<std::string>{"/usr/bin/bash", "--posix", "-i"}));
    EXPECT_EQ(options.environment["ENV"], kScripts + "/bash/cutefish-terminal.bash");
    EXPECT_EQ(options.environment["CUTEFISH_TERMINAL_BASH_INJECT"], "1 --norc");
    EXPECT_EQ(options.copies["CUTEFISH_TERMINAL_BASH_ENV"], "ENV");

    // A command, or POSIX mode asked for, is left alone.
    for (const auto& arguments : {std::vector<std::string>{"bash", "-c", "true"},
                                  std::vector<std::string>{"bash", "-lc", "true"},
                                  std::vector<std::string>{"bash", "--posix"}}) {
        Pty::Options plain = launch(arguments);
        EXPECT_FALSE(integrateShell(kScripts, plain));
        EXPECT_EQ(plain.arguments, arguments);
        EXPECT_TRUE(plain.environment.empty());
    }
}

TEST(ShellIntegrationTest, ZshAndFishFindTheirScriptsThroughTheEnvironment)
{
    Pty::Options zsh = launch({"/usr/bin/zsh"});
    ASSERT_TRUE(integrateShell(kScripts, zsh));
    EXPECT_EQ(zsh.arguments, std::vector<std::string>{"/usr/bin/zsh"});
    EXPECT_EQ(zsh.environment["ZDOTDIR"], kScripts + "/zsh");
    EXPECT_EQ(zsh.copies["CUTEFISH_TERMINAL_ZSH_ZDOTDIR"], "ZDOTDIR");

    Pty::Options fish = launch({"fish"});
    ASSERT_TRUE(integrateShell(kScripts, fish));
    EXPECT_EQ(fish.environment["CUTEFISH_TERMINAL_SHELL_INTEGRATION_XDG_DIR"], kScripts);
    EXPECT_EQ(fish.prefixes["XDG_DATA_DIRS"].first, kScripts);

    Pty::Options sh = launch({"/bin/sh"});
    EXPECT_FALSE(integrateShell(kScripts, sh));
    Pty::Options missing = launch({"/usr/bin/bash"});
    EXPECT_FALSE(integrateShell("/nonexistent", missing));
}

// Runs the loop until the condition holds, for at most five seconds.
static bool waitFor(cutefish::base::EventLoop& loop, const std::function<bool()>& condition)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!condition()) {
        if (std::chrono::steady_clock::now() > deadline)
            return false;
        cutefish::base::Timer tick;
        tick.setSingleShot(true);
        tick.onTimeout = [] {};
        tick.start(std::chrono::milliseconds(10));
        loop.dispatch();
    }
    return true;
}

static bool shows(Session& session, const std::string& text)
{
    Terminal& terminal = session.terminal();
    for (int line = 0; line < terminal.lines(); ++line) {
        if (terminal.logicalLine(line).text.find(text) != std::string::npos)
            return true;
    }
    return false;
}

// A home of the test's own for the shell's startup files, set for this
// process only while the test runs.
class ShellTest : public testing::TestWithParam<const char*>
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_home.isValid());
        if (const char* home = std::getenv("HOME"))
            m_savedHome = home;
        ::setenv("HOME", m_home.path().c_str(), 1);
        ::unsetenv("ZDOTDIR");
        ::unsetenv("XDG_CONFIG_HOME");
        Session::setShellIntegrationDirectory(kScripts);
    }

    void TearDown() override
    {
        if (m_savedHome)
            ::setenv("HOME", m_savedHome->c_str(), 1);
    }

    void write(const std::string& name, const std::string& text)
    {
        const std::filesystem::path path = std::filesystem::path(m_home.path()) / name;
        std::filesystem::create_directories(path.parent_path());
        ASSERT_TRUE(cutefish::base::file::writeAtomically(path.string(), text));
    }

    cutefish::base::file::TemporaryDirectory m_home{"/tmp", "shell-integration-home"};
    std::optional<std::string> m_savedHome;
};

TEST_P(ShellTest, MarksPromptsReportsTheDirectoryAndKeepsTheUsersStartup)
{
    const std::string shell = GetParam();
    const std::string path = "/usr/bin/" + shell;
    ASSERT_EQ(::access(path.c_str(), X_OK), 0) << path << " is needed for this test";
    write(".bashrc", "CFT_RC=loaded\n");
    write(".zshrc", "CFT_RC=loaded\n");
    write(".config/fish/config.fish", "set CFT_RC loaded\n");

    cutefish::base::EventLoop loop;
    Session session;
    session.setCommand({path});
    session.setInitialWorkingDirectory(m_home.path());
    session.start();
    Terminal& terminal = session.terminal();

    // The line the text is last on, or -1.
    const auto lineOf = [&](const std::string& text) {
        for (int line = terminal.lines() - 1; line >= 0; --line) {
            if (terminal.logicalLine(line).text.find(text) != std::string::npos)
                return line;
        }
        return -1;
    };
    // A prompt below the text.
    const auto promptAfter = [&](const std::string& text) {
        return waitFor(loop, [&] {
            const int line = lineOf(text);
            return line >= 0 && terminal.cursorAtPrompt() && terminal.cursorLine() > line;
        });
    };

    // At the prompt, in the home directory, with the user's file read and
    // nothing of the integration left in the environment.
    EXPECT_TRUE(waitFor(loop, [&] { return terminal.cursorAtPrompt(); }));
    EXPECT_TRUE(waitFor(loop, [&] { return terminal.reportedDirectory() == m_home.path(); }))
            << terminal.reportedDirectory();
    session.sendText("echo \"rc=$CFT_RC env=$ENV zdotdir=$ZDOTDIR xdg=$XDG_DATA_DIRS\"\r");
    EXPECT_TRUE(waitFor(loop, [&] { return shows(session, "rc=loaded env= zdotdir= xdg="); }));
    EXPECT_FALSE(shows(session, "xdg=" TERMINAL_SOURCE_DIR));
    EXPECT_TRUE(promptAfter("rc=loaded"));
    // POSIX mode would have kept the history in ~/.sh_history.
    if (shell == "bash") {
        session.sendText("echo \"hist=$HISTFILE\"\r");
        EXPECT_TRUE(waitFor(loop, [&] { return shows(session, "hist=" + m_home.path() + "/.bash_history"); }));
        EXPECT_TRUE(promptAfter("hist="));
    }

    // Output is not the prompt.
    session.sendText("sleep 30\r");
    EXPECT_TRUE(waitFor(loop, [&] { return !terminal.cursorAtPrompt(); }));
    // Interrupted once it runs, not while the shell still starts it.
    EXPECT_TRUE(waitFor(loop, [&] { return session.foregroundProcessName() == "sleep"; }));
    session.sendText("\x03");
    EXPECT_TRUE(promptAfter("sleep 30"));

    // Another directory is reported.
    session.sendText("cd /tmp\r");
    EXPECT_TRUE(waitFor(loop, [&] { return terminal.reportedDirectory() == "/tmp"; }))
            << terminal.reportedDirectory();
}

INSTANTIATE_TEST_SUITE_P(Shells, ShellTest, testing::Values("bash", "zsh", "fish"));
