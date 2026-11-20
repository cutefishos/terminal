// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

// Throughput of the terminal: program output fed in pty-sized chunks, as
// the pty delivers it, with no display; or "pty FILE": the file through a
// real pty and session, the render state taken after every read as the view
// takes it.
#include <chrono>
#include <clocale>
#include <cstdio>
#include <filesystem>
#include <malloc.h>
#include <random>
#include <string>
#include <unistd.h>
#include <vector>

#include "base/event_loop.h"

#include "session.h"
#include "terminal.h"

using Clock = std::chrono::steady_clock;

// A Mersenne twister on a seed_seq of one value: the same stream on every run.
class Random
{
public:
    explicit Random(uint32_t seed)
    {
        std::seed_seq sequence{seed};
        m_engine.seed(sequence);
    }

    int bounded(int highest)
    {
        uint64_t value = m_engine();
        value *= uint32_t(highest);
        value /= uint64_t(0xffffffff) + 1;
        return int(value);
    }

private:
    std::mt19937 m_engine;
};

static std::string workload(const std::string& kind, size_t bytes)
{
    std::string out;
    out.reserve(bytes + 4096);
    Random rng(42);
    static const char* words[] = {"error", "warning", "build", "src/main.cpp", "Linking", "CXX",
                                  "object", "the", "a", "function", "return", "value", "int",
                                  "QString", "const", "auto", "[100%]", "make[2]:", "-O2", "-Wall"};
    int line = 0;
    while (out.size() < bytes) {
        if (kind == "plain" || kind == "color") {
            const int count = 3 + rng.bounded(14);
            for (int i = 0; i < count; ++i) {
                const char* word = words[rng.bounded(20)];
                if (kind == "color" && rng.bounded(4) == 0) {
                    out += "\x1b[1;3" + std::to_string(1 + rng.bounded(6)) + "m";
                    out += word;
                    out += "\x1b[0m";
                } else {
                    out += word;
                }
                out += ' ';
            }
            out += "\r\n";
        } else if (kind == "utf8") {
            // CJK, Japanese, accented Latin, a dash, a check mark and Hangul.
            out += "\xe7\xac\xac" + std::to_string(line)
                    + "\xe8\xa1\x8c \xe4\xb8\xad\xe6\x96\x87\xe8\xbe\x93\xe5\x87\xba "
                      "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe3\x81\xae\xe3\x83\x86\xe3\x82\xad\xe3\x82\xb9\xe3\x83\x88 "
                      "\xc3\x9cn\xc3\xaf" "c\xc3\xb6" "d\xc3\xa9 \xe2\x80\x94 r\xc3\xa9sum\xc3\xa9 \xe2\x9c\x93 "
                      "\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4\r\n";
        } else if (kind == "short") {
            // seq-style output: a short line per number.
            out += std::to_string(line) + "\r\n";
        } else if (kind == "redraw") {
            // A full-screen program repainting every row in place, without scrolling.
            const int row = line % 52;
            out += "\x1b[" + std::to_string(row + 1) + ";1H\x1b[3" + std::to_string(row % 8) + "m";
            for (int i = 0; i < 170; ++i)
                out += char('a' + (line + i) % 26);
            out += "\x1b[0m";
        } else if (kind == "long") {
            for (int i = 0; i < 1500; ++i)
                out += char('a' + rng.bounded(26));
            out += "\r\n";
        }
        ++line;
    }
    return out;
}

// Resident memory, since the terminal's pages are mapped rather than taken from the heap.
static double residentMegabytes()
{
    long pages = 0;
    long resident = 0;
    if (FILE* statm = std::fopen("/proc/self/statm", "r")) {
        if (std::fscanf(statm, "%ld %ld", &pages, &resident) != 2)
            resident = 0;
        std::fclose(statm);
    }
    return double(resident) * double(sysconf(_SC_PAGESIZE)) / 1048576.0;
}

static double seconds(Clock::time_point start)
{
    return std::chrono::duration<double>(Clock::now() - start).count();
}

static int throughPty(const std::string& file)
{
    cutefish::base::EventLoop loop;
    term::Session session;
    session.setScrollbackLines(10000);
    session.resize(175, 52, 8, 16);
    session.setCommand({"/bin/cat", file});
    GhosttyRenderState state = nullptr;
    ghostty_render_state_new(nullptr, &state);
    auto output = session.outputReceived.connect([&] {
        ghostty_render_state_update(state, session.terminal().handle());
        ghostty_render_state_clean(state);
    });
    const Clock::time_point start = Clock::now();
    auto finished = session.finished.connect([&] {
        const double megabytes = double(std::filesystem::file_size(file)) / 1048576.0;
        const double elapsed = seconds(start);
        std::printf("cat %.1f MB through the pty: %.0f ms, %.1f MB/s\n", megabytes, elapsed * 1000,
                    megabytes / elapsed);
        loop.quit();
    });
    session.start();
    const int result = loop.run();
    ghostty_render_state_free(state);
    return result;
}

int main(int argc, char** argv)
{
    std::setlocale(LC_ALL, "");
    std::vector<std::string> kinds(argv + 1, argv + argc);
    if (kinds.size() == 2 && kinds.front() == "pty")
        return throughPty(kinds.back());
    if (kinds.empty())
        kinds = {"plain", "color", "utf8", "long", "short", "redraw"};

    for (const std::string& kind : kinds) {
        const std::string data = workload(kind, 32 * 1024 * 1024);
        term::Terminal terminal(175, 52);
        terminal.setScrollbackLines(10000);

        const Clock::time_point start = Clock::now();
        for (size_t offset = 0; offset < data.size(); offset += 4096)
            terminal.write(std::string_view(data).substr(offset, 4096));
        const double elapsed = seconds(start);
        const double megabytes = double(data.size()) / 1048576.0;
        std::printf("%-6s %6.1f MB in %7.1f ms  %6.1f MB/s  heap in use %.1f MB, resident %.1f MB\n", kind.c_str(),
                    megabytes, elapsed * 1000, megabytes / elapsed, double(mallinfo2().uordblks) / 1048576.0,
                    residentMegabytes());
        std::fflush(stdout);
    }
    return 0;
}
