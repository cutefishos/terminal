// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <vector>

namespace cutefish::base {
class EventLoop;
}

namespace term {

// A program on a pseudo terminal: the program leads a session of its own with
// the pty as its controlling terminal and its standard streams. Output and the
// exit arrive on the event loop; writes queue and go out as the pty takes them.
class Pty
{
public:
    struct Options
    {
        // A path, or a name looked up on PATH.
        std::string program;
        // argv, the first entry included.
        std::vector<std::string> arguments;
        std::string workingDirectory;
        // On top of this process's environment: a value sets the variable,
        // none removes it.
        std::map<std::string, std::optional<std::string>> environment;
        // Set only where the environment has no such variable.
        std::map<std::string, std::string> defaults;
        int columns = 80;
        int lines = 24;
    };

    Pty();
    ~Pty();

    Pty(const Pty&) = delete;
    Pty& operator=(const Pty&) = delete;

    bool start(const Options& options);
    bool isRunning() const { return m_pid > 0; }
    pid_t processId() const { return m_pid; }
    // The process group in front on the terminal, or 0.
    pid_t foregroundProcessGroup() const;

    void resize(int columns, int lines, int pixelWidth = 0, int pixelHeight = 0);
    int columns() const { return m_columns; }
    int lines() const { return m_lines; }

    void write(std::string_view data);

    // SIGHUP, then SIGKILL when the program is still there after a second.
    // The exit is reported as usual.
    void terminate();

    std::function<void(const char* data, size_t length)> onOutput;
    // The exit status, and whether a signal ended the program.
    std::function<void(int status, bool signaled)> onExit;

private:
    void readOutput();
    void flush();
    void updateWriteWatch();
    bool waitForExit(int milliseconds);
    void reap();
    void closeMaster();

    cutefish::base::EventLoop& m_loop;
    int m_master = -1;
    int m_masterWatch = 0;
    bool m_watchingWrites = false;
    pid_t m_pid = 0;
    int m_pidFd = -1;
    int m_pidWatch = 0;
    int m_columns = 80;
    int m_lines = 24;
    std::string m_pending;
    std::vector<char> m_buffer;
};

}
