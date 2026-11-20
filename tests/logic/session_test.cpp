// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unistd.h>
#include <vector>

#include "base/event_loop.h"
#include "base/file.h"
#include "base/paths.h"
#include "base/timer.h"

#include "session.h"

using namespace term;

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

TEST(SessionTest, TitleFollowsTheProgramInFront)
{
    cutefish::base::EventLoop loop;
    cutefish::base::file::TemporaryDirectory directory("/tmp", "session-test");
    ASSERT_TRUE(directory.isValid());
    const std::string directoryName = std::filesystem::path(directory.path()).filename().string();

    Session session;
    session.setCommand({"/bin/sh"});
    session.setInitialWorkingDirectory(directory.path());

    // What a tab shows each time it hears the title changed.
    std::vector<std::string> titles;
    auto subscription = session.titleChanged.connect([&] { titles.push_back(session.title()); });

    session.start();
    EXPECT_TRUE(waitFor(loop, [&] { return session.title() == directoryName; }));

    // A program that sets no title shows its name, and the prompt its directory.
    session.sendText("sleep 30\r");
    EXPECT_TRUE(waitFor(loop, [&] { return std::count(titles.begin(), titles.end(), "sleep") > 0; }));
    session.sendText("\x03");
    EXPECT_TRUE(waitFor(loop, [&] { return !titles.empty() && titles.back() == directoryName; }));

    // A title the program sets wins over its name while it runs.
    session.sendText("sh -c 'printf \"\\033]2;hello\\007\"; sleep 30'\r");
    EXPECT_TRUE(waitFor(loop, [&] { return !titles.empty() && titles.back() == "hello"; }));
    session.sendText("\x03");
    EXPECT_TRUE(waitFor(loop, [&] { return !titles.empty() && titles.back() == directoryName; }));

    // A title the shell sets at its prompt does not replace the directory.
    const size_t heard = titles.size();
    session.sendText("printf '\\033]0;user@host: ~\\007'\r");
    EXPECT_TRUE(waitFor(loop, [&] { return titles.size() > heard; }));
    ASSERT_FALSE(titles.empty());
    EXPECT_EQ(titles.back(), directoryName);
}

TEST(SessionTest, ShellExitFinishesTheSession)
{
    cutefish::base::EventLoop loop;
    Session session;
    session.setCommand({"/bin/sh"});
    bool finished = false;
    auto subscription = session.finished.connect([&] { finished = true; });

    session.start();
    ASSERT_TRUE(session.isRunning());
    session.sendText("exit\r");
    EXPECT_TRUE(waitFor(loop, [&] { return finished; }));
    EXPECT_FALSE(session.isRunning());
}

TEST(SessionTest, LastOutputArrivesBeforeTheEnd)
{
    cutefish::base::EventLoop loop;
    Session session;
    session.setCommand({"/bin/sh", "-c", "printf 'goodbye'"});
    bool finished = false;
    std::string screen;
    auto subscription = session.finished.connect([&] {
        finished = true;
        screen = session.terminal().logicalLine(0).text;
    });
    session.start();
    EXPECT_TRUE(waitFor(loop, [&] { return finished; }));
    EXPECT_TRUE(screen.starts_with("goodbye")) << screen;
}

TEST(SessionTest, ChildSeesTheTerminalEnvironment)
{
    cutefish::base::EventLoop loop;
    cutefish::base::file::TemporaryDirectory directory("/tmp", "session-env");
    ASSERT_TRUE(directory.isValid());
    const std::string output = directory.path() + "/env";

    Session session;
    session.setCommand({"/bin/sh", "-c",
                        "printf '%s|%s|%s|%s|%s|%s' \"$TERM\" \"$COLORTERM\" \"$COLORFGBG\" \"$WINDOWID\" "
                        "\"${LANGUAGE-unset}\" \"$(tty)\" > "
                                + output});
    session.start();

    std::optional<std::string> contents;
    EXPECT_TRUE(waitFor(loop, [&] {
        contents = cutefish::base::file::read(output);
        return contents && contents->find("/dev/pts/") != std::string::npos;
    }));
    ASSERT_TRUE(contents);
    EXPECT_TRUE(contents->starts_with("xterm-256color|truecolor|15;0|0|")) << *contents;
    EXPECT_EQ(contents->find("unset"), std::string::npos) << *contents;
}

TEST(SessionTest, CurrentDirBehindAProgramOfAnotherUser)
{
    // Set-group-ID, so its working directory is not ours to read.
    const std::string agent = cutefish::base::paths::findExecutable("ssh-agent");
    if (agent.empty())
        GTEST_SKIP() << "no ssh-agent";
    cutefish::base::EventLoop loop;
    cutefish::base::file::TemporaryDirectory directory("/tmp", "session-cwd");
    ASSERT_TRUE(directory.isValid());

    Session session;
    session.setCommand({"/bin/sh"});
    session.setInitialWorkingDirectory(directory.path());
    session.start();
    ASSERT_TRUE(waitFor(loop, [&] { return session.currentDir() == directory.path(); }));

    // Looked at only once the agent runs: its socket is there.
    const std::string socket = directory.path() + "/agent";
    session.sendText(agent + " -D -a " + socket + "\r");
    ASSERT_TRUE(waitFor(loop, [&] { return std::filesystem::exists(socket); }));
    ASSERT_EQ(session.foregroundProcessName(), "ssh-agent");
    EXPECT_EQ(session.currentDir(), directory.path());
    session.sendText("\x03");
}

TEST(SessionTest, ClearAllAtAPromptAsksTheShellToRedraw)
{
    cutefish::base::EventLoop loop;
    cutefish::base::file::TemporaryDirectory directory("/tmp", "session-clear");
    ASSERT_TRUE(directory.isValid());
    const std::string output = directory.path() + "/received";

    // A program in front of nothing: it reads what the terminal sends.
    Session session;
    session.setCommand({"/bin/sh", "-c", "stty raw -echo; exec cat > '" + output + "'"});
    session.start();
    ASSERT_TRUE(waitFor(loop, [&] { return session.foregroundProcessGroup() == session.processId(); }));
    session.terminal().write("a\r\nb\r\n$ ls");
    EXPECT_TRUE(session.clearScreenAndScrollback());
    EXPECT_TRUE(waitFor(loop, [&] { return cutefish::base::file::read(output).value_or("") == "\x0c"; }));

    session.terminal().write("\033[?1049h");
    EXPECT_FALSE(session.clearScreenAndScrollback());
}

TEST(SessionTest, DirectoriesExpandAndFallBack)
{
    cutefish::base::EventLoop loop;
    const std::string output = "/tmp/session-dir-" + std::to_string(::getpid());
    Session session;
    session.setInitialWorkingDirectory("/nonexistent/directory");
    session.setCommand({"/bin/sh", "-c", "pwd > " + output});
    bool finished = false;
    auto subscription = session.finished.connect([&] { finished = true; });
    session.start();
    EXPECT_TRUE(waitFor(loop, [&] { return finished; }));
    EXPECT_EQ(cutefish::base::file::read(output).value_or(""), cutefish::base::paths::home() + "\n");
    std::filesystem::remove(output);
}
