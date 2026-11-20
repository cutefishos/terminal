// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <string>

#include <ghostty/vt.h>

namespace term {

class Terminal;

// A find bar's search through a terminal's screen and scrollback. The match it
// lands on becomes the terminal's selection, scrolled into view.
class Search
{
public:
    explicit Search(Terminal& terminal);
    ~Search();

    Search(const Search&) = delete;
    Search& operator=(const Search&) = delete;

    // To the next match towards older output, or newer when `forwards`; a new
    // text starts again from the newest. Letters compare in either case
    // unless `caseSensitive`. False when nothing matches.
    bool find(const std::string& text, bool forwards, bool caseSensitive);

private:
    Terminal& m_terminal;
    GhosttySearch m_search = nullptr;
    std::string m_text;
};

}
