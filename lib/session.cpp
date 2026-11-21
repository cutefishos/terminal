// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include "session.h"

#include <climits>
#include <filesystem>
#include <optional>
#include <pwd.h>
#include <unistd.h>

#include "base/file.h"
#include "base/paths.h"
#include "base/strings.h"

#include "proxyenvironment.h"
#include "shellintegration.h"

namespace strings = cutefish::base::strings;

namespace term {

static std::string processDirectory()
{
    std::error_code error;
    const std::filesystem::path path = std::filesystem::current_path(error);
    return error ? std::string() : path.string();
}

static std::string passwdShell()
{
    const passwd* entry = ::getpwuid(::getuid());
    return entry && entry->pw_shell && *entry->pw_shell ? std::string(entry->pw_shell) : std::string();
}

static bool isExecutable(const std::string& path)
{
    return !path.empty() && ::access(path.c_str(), X_OK) == 0;
}

// "$HOME" and "$PWD" where they start a path or stand alone.
static std::string expandDirectory(const std::string& text)
{
    for (const auto& [name, value] : {std::pair<std::string, std::string>{"$HOME", cutefish::base::paths::home()},
                                      std::pair<std::string, std::string>{"$PWD", processDirectory()}}) {
        if (text == name || text.starts_with(name + "/"))
            return value + text.substr(name.size());
    }
    return text;
}

// The link of a process's working directory, which only its own user can read.
static std::optional<std::string> directoryOf(int pid)
{
    char path[PATH_MAX];
    const ssize_t length = ::readlink(("/proc/" + std::to_string(pid) + "/cwd").c_str(), path, sizeof(path));
    if (length < 0)
        return std::nullopt;
    return strings::sanitizedUtf8(std::string_view(path, size_t(length)));
}

Session::Session()
{
    m_terminal.onSend = [this](std::string_view bytes) {
        m_pty.write(bytes);
        checkForegroundLater();
    };
    // The shell may hand the terminal to a program just after the program
    // set its title, so whose title it is gets looked at a moment later.
    m_titleLater.setSingleShot(true);
    m_titleLater.setInterval(std::chrono::milliseconds(20));
    m_titleLater.onTimeout = [this] {
        m_programTitle = m_terminal.title();
        m_programTitleGroup = foregroundProcessGroup();
        notifyTitle();
    };
    m_terminal.onTitleChanged = [this] {
        if (!m_titleLater.isActive())
            m_titleLater.start();
    };
    m_terminal.onBell = [this] { bell(); };

    m_pty.onOutput = [this](const char* data, size_t length) {
        m_terminal.write(std::string_view(data, length));
        checkForegroundLater();
        outputReceived();
    };
    m_pty.onExit = [this](int, bool signaled) { exited(signaled); };

    // Nothing announces a program starting or ending, so the foreground is
    // looked at after output or input, at most every 250 ms.
    m_foregroundCheck.setSingleShot(true);
    m_foregroundCheck.setInterval(std::chrono::milliseconds(250));
    m_foregroundCheck.onTimeout = [this] {
        if (foregroundProcessGroup() != m_titledGroup)
            notifyTitle();
    };
    m_finishLater.setSingleShot(true);
    m_finishLater.onTimeout = [this] { finished(); };
}

Session::~Session()
{
    m_destroying = true;
    close();
}

static std::string& shellIntegrationDirectory()
{
    static std::string directory = SHELL_INTEGRATION_DIR;
    return directory;
}

void Session::setShellIntegrationDirectory(const std::string& directory)
{
    shellIntegrationDirectory() = directory;
}

void Session::setInitialWorkingDirectory(const std::string& directory)
{
    std::string path = expandDirectory(directory);
    if (path.empty())
        path = processDirectory();
    std::error_code error;
    if (!std::filesystem::is_directory(path, error))
        path = cutefish::base::paths::home();
    m_workingDirectory = path;
}

void Session::setCommand(const std::vector<std::string>& command)
{
    m_command = command;
}

void Session::setScrollbackLines(int lines)
{
    m_terminal.setScrollbackLines(lines);
}

void Session::start()
{
    if (isRunning())
        return;

    std::vector<std::string> arguments = m_command;
    std::string program = arguments.empty() ? std::string() : arguments.front();
    // A path that is not there runs the user's shell instead, then /bin/sh;
    // a bare name is looked up on PATH.
    if (program.empty() || (program.starts_with('/') && !isExecutable(program))) {
        program = passwdShell();
        if (!isExecutable(program))
            program = "/bin/sh";
        arguments = {program};
    }

    Pty::Options options;
    options.program = program;
    options.arguments = arguments;
    options.workingDirectory = m_workingDirectory.empty() ? processDirectory() : m_workingDirectory;
    options.columns = m_terminal.columns();
    options.lines = m_terminal.lines();
    options.environment = proxyEnvironment();
    options.environment["TERM"] = "xterm-256color";
    options.environment["COLORTERM"] = "truecolor";
    options.environment["COLORFGBG"] = "15;0";
    options.environment["WINDOWID"] = "0";
    options.defaults["LANGUAGE"] = "";
    integrateShell(shellIntegrationDirectory(), options);
    if (m_pty.start(options))
        checkForegroundLater();
}

void Session::resize(int columns, int lines, int cellWidth, int cellHeight)
{
    if (columns == m_terminal.columns() && lines == m_terminal.lines() && cellWidth == m_terminal.cellWidth()
        && cellHeight == m_terminal.cellHeight()) {
        return;
    }
    m_terminal.resize(columns, lines, cellWidth, cellHeight);
    m_pty.resize(columns, lines, columns * cellWidth, lines * cellHeight);
}

void Session::exited(bool signaled)
{
    if (m_destroying)
        return;
    // A shell that crashed by itself keeps its tab, with what it wrote last.
    if (signaled && !m_closing)
        return;
    finished();
}

const Session::Foreground& Session::foreground()
{
    const int group = foregroundProcessGroup();
    m_foreground = Foreground{group};
    if (group <= 0)
        return m_foreground;

    // /proc/<pid>/stat: "pid (name) state ppid ...", the name in the outermost parentheses.
    const std::optional<std::string> stat = cutefish::base::file::read("/proc/" + std::to_string(group) + "/stat");
    if (!stat)
        return m_foreground;
    const size_t open = stat->find('(');
    const size_t close = stat->rfind(')');
    if (open == std::string::npos || close == std::string::npos || close < open)
        return m_foreground;
    m_foreground.name = strings::sanitizedUtf8(std::string_view(*stat).substr(open + 1, close - open - 1));
    m_foreground.valid = true;
    if (std::optional<std::string> directory = directoryOf(group)) {
        m_foreground.directory = *directory;
        m_foreground.hasDirectory = true;
    }
    return m_foreground;
}

std::string Session::foregroundProcessName()
{
    const Foreground& front = foreground();
    return front.valid ? front.name : std::string();
}

std::string Session::currentDir()
{
    const Foreground& front = foreground();
    if (front.valid && front.hasDirectory)
        return front.directory;
    // A program of another user in front, such as su, hides its directory; the shell's stands in.
    if (processId() > 0 && processId() != front.group)
        return directoryOf(processId()).value_or(std::string());
    return {};
}

bool Session::hasActiveProcess() const
{
    return processId() != foregroundProcessGroup();
}

std::string Session::title()
{
    const int front = foregroundProcessGroup();
    if (front > 0 && front != processId()) {
        if (front == m_programTitleGroup && !m_programTitle.empty())
            return m_programTitle;
        const std::string name = foregroundProcessName();
        if (!name.empty())
            return name;
    }

    std::string directory = currentDir();
    std::string home = cutefish::base::paths::home();
    while (home.size() > 1 && home.back() == '/')
        home.pop_back();
    while (directory.size() > 1 && directory.back() == '/')
        directory.pop_back();
    if (directory == home || directory == "/")
        return directory;
    const size_t slash = directory.rfind('/');
    return slash == std::string::npos ? directory : directory.substr(slash + 1);
}

void Session::checkForegroundLater()
{
    if (!m_foregroundCheck.isActive())
        m_foregroundCheck.start();
}

void Session::notifyTitle()
{
    m_titledGroup = foregroundProcessGroup();
    titleChanged();
}

void Session::sendText(std::string_view text)
{
    m_terminal.send(text);
}

void Session::clearScrollback()
{
    m_terminal.clearScrollback();
    outputReceived();
}

bool Session::isAtPrompt() const
{
    return m_terminal.hasPromptMarks() ? m_terminal.cursorAtPrompt() : !hasActiveProcess();
}

bool Session::needsConfirmClose() const
{
    return isRunning() && !isAtPrompt();
}

bool Session::clearScreenAndScrollback()
{
    if (m_terminal.usesAlternateScreen())
        return false;
    if (isAtPrompt()) {
        m_terminal.clearScreen();
        // Ctrl+L: the shell draws its prompt and what was typed again.
        m_terminal.send("\x0c");
    } else if (m_terminal.hasPromptMarks()) {
        m_terminal.clearAboveCursor();
    } else {
        m_terminal.clearKeepingCursorLine();
    }
    outputReceived();
    return true;
}

void Session::resetTerminal()
{
    m_terminal.reset();
    // A size change makes the shell or a full-screen program draw itself again,
    // where Ctrl+L would only clear the screen.
    const int columns = m_terminal.columns();
    const int lines = m_terminal.lines();
    m_pty.resize(columns + 1, lines);
    m_pty.resize(columns, lines, columns * m_terminal.cellWidth(), lines * m_terminal.cellHeight());
    outputReceived();
}

void Session::close()
{
    m_closing = true;
    if (!isRunning()) {
        if (!m_destroying)
            m_finishLater.start(std::chrono::milliseconds(1));
        return;
    }
    m_pty.terminate();
    // A program that outlives SIGKILL is not waited for.
    if (isRunning() && !m_destroying)
        m_finishLater.start(std::chrono::milliseconds(1));
}

}
