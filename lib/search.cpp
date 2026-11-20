// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include "search.h"

#include "terminal.h"

namespace term {

Search::Search(Terminal& terminal)
    : m_terminal(terminal)
{
    ghostty_search_new(nullptr, &m_search, terminal.handle());
}

Search::~Search()
{
    ghostty_search_free(m_search);
}

bool Search::find(const std::string& text, bool forwards, bool caseSensitive)
{
    if (!m_search)
        return false;
    if (text.empty()) {
        m_text.clear();
        ghostty_search_set(m_search, GHOSTTY_SEARCH_OPT_NEEDLE, nullptr);
        m_terminal.clearSelection();
        return false;
    }

    const bool restart = text != m_text;
    m_text = text;
    const GhosttyString needle{reinterpret_cast<const uint8_t*>(text.data()), text.size()};
    ghostty_search_set(m_search, GHOSTTY_SEARCH_OPT_NEEDLE, &needle);
    if (ghostty_search_run(m_search) != GHOSTTY_SUCCESS)
        return false;

    size_t total = 0;
    ghostty_search_get(m_search, GHOSTTY_SEARCH_DATA_TOTAL_MATCHES, &total);
    // The library ignores the case of ASCII letters, so a case-sensitive search
    // steps over the matches that differ in case, at most once round.
    for (size_t step = 0; step < total; ++step) {
        const GhosttyResult moved = forwards && !restart ? ghostty_search_select_prev(m_search)
                                                         : ghostty_search_select_next(m_search);
        if (moved != GHOSTTY_SUCCESS)
            break;
        GhosttySelection match{};
        if (ghostty_search_get(m_search, GHOSTTY_SEARCH_DATA_SELECTED_MATCH, &match) != GHOSTTY_SUCCESS)
            break;
        if (caseSensitive && m_terminal.text(match) != text)
            continue;
        m_terminal.setSelection(match);
        return true;
    }
    m_terminal.clearSelection();
    return false;
}

}
