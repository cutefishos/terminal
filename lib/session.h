// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/signal.h"
#include "base/timer.h"

#include "pty.h"
#include "terminal.h"

namespace term {

// One tab's program on a pty, with the terminal its output goes into, and
// what a tab shows of it: a title and the working directory.
class Session
{
public:
    Session();
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    Terminal& terminal() { return m_terminal; }
    const Terminal& terminal() const { return m_terminal; }

    // Where the shells' integration scripts are, for every session; the
    // installed ones unless set.
    static void setShellIntegrationDirectory(const std::string& directory);

    // "$PWD" is this process's directory and "$HOME" the home directory; a
    // directory that does not exist gives the home directory.
    void setInitialWorkingDirectory(const std::string& directory);
    // A program and its arguments; empty runs the user's shell from passwd.
    void setCommand(const std::vector<std::string>& command);
    // Negative keeps everything.
    void setScrollbackLines(int lines);

    void start();
    bool isRunning() const { return m_pty.isRunning(); }
    int processId() const { return int(m_pty.processId()); }
    int foregroundProcessGroup() const { return int(m_pty.foregroundProcessGroup()); }

    void resize(int columns, int lines, int cellWidth, int cellHeight);

    // The program in front: the title it set, or its name; at the prompt the
    // working directory's name, home and root in full.
    std::string title();
    // A program runs in front of the shell.
    bool hasActiveProcess() const;
    std::string foregroundProcessName();
    std::string currentDir();
    // Where a new tab or window from this one starts: the directory the shell
    // reported (OSC 7) while it is there, else currentDir().
    std::string inheritedDirectory();

    // Bytes for the program as they are.
    void sendText(std::string_view text);

    // At the shell's prompt: as the shell marks it, or else with no program
    // in front of the shell.
    bool isAtPrompt() const;
    // Closing would stop something that runs: not at the prompt.
    bool needsConfirmClose() const;

    void clearScrollback();
    // False on the alternate screen, which keeps the key for its program. At
    // a prompt the shell is asked to draw its prompt again; elsewhere the
    // lines above the cursor go, or with no prompt marks the cursor's line
    // moves to the top.
    bool clearScreenAndScrollback();
    // A full reset; the program is told the size again so it redraws.
    void resetTerminal();

    // Hangs up the program; finished follows.
    void close();

    cutefish::base::Signal<> finished;
    cutefish::base::Signal<> titleChanged;
    cutefish::base::Signal<> bell;
    // After each read of program output.
    cutefish::base::Signal<> outputReceived;

private:
    struct Foreground
    {
        int group = 0;
        bool valid = false;
        std::string name;
        std::string directory;
        bool hasDirectory = false;
    };

    void exited(bool signaled);
    const Foreground& foreground();
    void checkForegroundLater();
    void notifyTitle();

    Terminal m_terminal;
    Pty m_pty;
    std::string m_workingDirectory;
    std::vector<std::string> m_command;
    Foreground m_foreground;

    // The last OSC 0 or 2 title and the process group in front when it came;
    // it titles the tab while that group stays in front.
    std::string m_programTitle;
    int m_programTitleGroup = 0;
    int m_titledGroup = 0;

    bool m_closing = false;
    bool m_destroying = false;
    cutefish::base::Timer m_foregroundCheck;
    cutefish::base::Timer m_titleLater;
    cutefish::base::Timer m_finishLater;
};

}
