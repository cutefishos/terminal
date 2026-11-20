// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include "pty.h"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <linux/close_range.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "base/event_loop.h"
#include "base/paths.h"

extern char** environ;

namespace term {

// What one wakeup reads at most, so a flood of output still lets the loop draw.
static constexpr size_t kReadLimit = 256 * 1024;
static constexpr size_t kReadChunk = 64 * 1024;

Pty::Pty()
    : m_loop(*cutefish::base::EventLoop::current())
{
}

Pty::~Pty()
{
    if (isRunning())
        terminate();
    closeMaster();
    if (m_pidWatch)
        m_loop.unwatch(m_pidWatch);
    if (m_pidFd >= 0)
        ::close(m_pidFd);
}

static std::vector<std::string> childEnvironment(const Pty::Options& options)
{
    std::map<std::string, std::string> variables;
    for (char** entry = environ; entry && *entry; ++entry) {
        const std::string_view text(*entry);
        const size_t equals = text.find('=');
        if (equals != std::string_view::npos && equals > 0)
            variables.emplace(std::string(text.substr(0, equals)), std::string(text.substr(equals + 1)));
    }
    for (const auto& [name, value] : options.environment) {
        if (value)
            variables[name] = *value;
        else
            variables.erase(name);
    }
    for (const auto& [name, value] : options.defaults)
        variables.emplace(name, value);

    std::vector<std::string> result;
    result.reserve(variables.size());
    for (const auto& [name, value] : variables)
        result.push_back(name + "=" + value);
    return result;
}

static std::vector<char*> pointers(std::vector<std::string>& strings)
{
    std::vector<char*> result;
    result.reserve(strings.size() + 1);
    for (std::string& string : strings)
        result.push_back(string.data());
    result.push_back(nullptr);
    return result;
}

bool Pty::start(const Options& options)
{
    if (isRunning())
        return false;

    std::string program = options.program;
    if (program.find('/') == std::string::npos)
        program = cutefish::base::paths::findExecutable(program);
    if (program.empty() || ::access(program.c_str(), X_OK) != 0)
        return false;

    const int master = ::posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (master < 0)
        return false;
    char name[128];
    if (::grantpt(master) != 0 || ::unlockpt(master) != 0 || ::ptsname_r(master, name, sizeof(name)) != 0) {
        ::close(master);
        return false;
    }
    const int slave = ::open(name, O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (slave < 0) {
        ::close(master);
        return false;
    }

    termios attributes{};
    if (::tcgetattr(slave, &attributes) == 0) {
        attributes.c_iflag |= IUTF8 | IXON;
        attributes.c_cc[VERASE] = 0x7f;
        ::tcsetattr(slave, TCSANOW, &attributes);
    }
    m_columns = std::max(1, options.columns);
    m_lines = std::max(1, options.lines);
    winsize size{};
    size.ws_col = uint16_t(m_columns);
    size.ws_row = uint16_t(m_lines);
    ::ioctl(slave, TIOCSWINSZ, &size);

    std::vector<std::string> arguments = options.arguments;
    if (arguments.empty())
        arguments.push_back(program);
    std::vector<std::string> environment = childEnvironment(options);
    std::vector<char*> argv = pointers(arguments);
    std::vector<char*> envp = pointers(environment);
    const std::string directory = options.workingDirectory;

    const pid_t pid = ::fork();
    if (pid < 0) {
        ::close(slave);
        ::close(master);
        return false;
    }
    if (pid == 0) {
        // Only async-signal-safe calls from here to exec.
        ::setsid();
        ::ioctl(slave, TIOCSCTTY, 0);
        ::dup2(slave, STDIN_FILENO);
        ::dup2(slave, STDOUT_FILENO);
        ::dup2(slave, STDERR_FILENO);
        ::close_range(3, ~0u, 0);

        sigset_t none;
        sigemptyset(&none);
        ::sigprocmask(SIG_SETMASK, &none, nullptr);
        for (int signal = 1; signal < NSIG; ++signal)
            ::signal(signal, SIG_DFL);

        if (!directory.empty() && ::chdir(directory.c_str()) != 0)
            static_cast<void>(::chdir("/"));
        ::execve(program.c_str(), argv.data(), envp.data());
        ::_exit(127);
    }

    ::close(slave);
    m_master = master;
    m_pid = pid;
    ::fcntl(m_master, F_SETFL, ::fcntl(m_master, F_GETFL) | O_NONBLOCK);
    m_masterWatch = m_loop.watch(m_master, EPOLLIN, [this](uint32_t events) {
        if (events & EPOLLOUT)
            flush();
        if (events & (EPOLLIN | EPOLLHUP | EPOLLERR))
            readOutput();
    });

    m_pidFd = int(::syscall(SYS_pidfd_open, pid, 0));
    if (m_pidFd >= 0)
        m_pidWatch = m_loop.watch(m_pidFd, EPOLLIN, [this](uint32_t) { reap(); });
    return true;
}

pid_t Pty::foregroundProcessGroup() const
{
    if (m_master < 0)
        return 0;
    const pid_t group = ::tcgetpgrp(m_master);
    return group > 0 ? group : 0;
}

void Pty::resize(int columns, int lines, int pixelWidth, int pixelHeight)
{
    m_columns = std::max(1, columns);
    m_lines = std::max(1, lines);
    if (m_master < 0)
        return;
    winsize size{};
    size.ws_col = uint16_t(m_columns);
    size.ws_row = uint16_t(m_lines);
    size.ws_xpixel = uint16_t(std::max(0, pixelWidth));
    size.ws_ypixel = uint16_t(std::max(0, pixelHeight));
    ::ioctl(m_master, TIOCSWINSZ, &size);
}

void Pty::write(std::string_view data)
{
    if (m_master < 0 || data.empty())
        return;
    m_pending.append(data);
    flush();
}

void Pty::flush()
{
    while (!m_pending.empty() && m_master >= 0) {
        const ssize_t written = ::write(m_master, m_pending.data(), m_pending.size());
        if (written > 0) {
            m_pending.erase(0, size_t(written));
            continue;
        }
        if (written < 0 && errno == EINTR)
            continue;
        if (written < 0 && errno != EAGAIN)
            m_pending.clear();
        break;
    }
    updateWriteWatch();
}

void Pty::updateWriteWatch()
{
    const bool wanted = !m_pending.empty() && m_master >= 0;
    if (wanted == m_watchingWrites || !m_masterWatch)
        return;
    m_watchingWrites = wanted;
    m_loop.setWatchEvents(m_masterWatch, wanted ? EPOLLIN | EPOLLOUT : EPOLLIN);
}

void Pty::readOutput()
{
    if (m_buffer.size() < kReadChunk)
        m_buffer.resize(kReadChunk);
    size_t total = 0;
    while (m_master >= 0 && total < kReadLimit) {
        const ssize_t length = ::read(m_master, m_buffer.data(), m_buffer.size());
        if (length > 0) {
            total += size_t(length);
            if (onOutput)
                onOutput(m_buffer.data(), size_t(length));
            continue;
        }
        if (length < 0 && errno == EINTR)
            continue;
        if (length < 0 && errno == EAGAIN)
            return;
        // EIO: every slave side is closed. The exit comes through the pidfd.
        if (m_masterWatch) {
            m_loop.unwatch(m_masterWatch);
            m_masterWatch = 0;
            m_watchingWrites = false;
        }
        return;
    }
}

bool Pty::waitForExit(int milliseconds)
{
    if (m_pidFd < 0)
        return false;
    pollfd descriptor{m_pidFd, POLLIN, 0};
    return ::poll(&descriptor, 1, milliseconds) > 0;
}

void Pty::reap()
{
    if (m_pid <= 0)
        return;
    int status = 0;
    const pid_t result = ::waitpid(m_pid, &status, WNOHANG);
    if (result == 0)
        return;

    // What the program wrote last is still in the pty.
    if (m_masterWatch)
        readOutput();

    m_pid = 0;
    if (m_pidWatch) {
        m_loop.unwatch(m_pidWatch);
        m_pidWatch = 0;
    }
    if (m_pidFd >= 0) {
        ::close(m_pidFd);
        m_pidFd = -1;
    }
    const bool signaled = result > 0 && WIFSIGNALED(status);
    const int code = result > 0 && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    if (onExit)
        onExit(signaled ? WTERMSIG(status) : code, signaled);
}

void Pty::terminate()
{
    if (m_pid <= 0)
        return;
    ::kill(m_pid, SIGHUP);
    if (!waitForExit(1000)) {
        closeMaster();
        ::kill(m_pid, SIGKILL);
        waitForExit(1000);
    }
    reap();
}

void Pty::closeMaster()
{
    if (m_masterWatch) {
        m_loop.unwatch(m_masterWatch);
        m_masterWatch = 0;
    }
    m_watchingWrites = false;
    m_pending.clear();
    if (m_master >= 0) {
        ::close(m_master);
        m_master = -1;
    }
}

}
