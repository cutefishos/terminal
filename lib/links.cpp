// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include "links.h"

#include <algorithm>

#include "base/strings.h"

namespace strings = cutefish::base::strings;

namespace term {

// What ends a link: space, and the quotes and brackets text puts around one.
static bool endsLink(char c)
{
    return static_cast<unsigned char>(c) <= ' ' || c == '"' || c == '\'' || c == '<' || c == '>' || c == '`'
            || c == 0x7f;
}

static bool isSchemeCharacter(char c)
{
    return strings::isAsciiAlpha(c) || strings::isAsciiDigit(c) || c == '+' || c == '-' || c == '.';
}

static bool isAddressCharacter(char c)
{
    return strings::isAsciiAlpha(c) || strings::isAsciiDigit(c) || c == '.' || c == '-' || c == '_' || c == '+'
            || c == '%';
}

// Punctuation after a link belongs to the sentence, and a closing bracket to
// the text around it unless the link opened one.
static size_t trimEnd(std::string_view text, size_t begin, size_t end)
{
    while (end > begin) {
        const char last = text[end - 1];
        if (std::string_view(".,:;!?").find(last) != std::string_view::npos) {
            --end;
            continue;
        }
        const char open = last == ')' ? '(' : last == ']' ? '[' : last == '}' ? '{' : 0;
        if (open) {
            const std::string_view link = text.substr(begin, end - begin);
            const auto opened = std::count(link.begin(), link.end(), open);
            const auto closed = std::count(link.begin(), link.end(), last);
            if (closed > opened) {
                --end;
                continue;
            }
        }
        break;
    }
    return end;
}

static Link webLink(std::string_view text, size_t offset)
{
    // The run of text around the offset that could hold a link.
    size_t runBegin = offset;
    while (runBegin > 0 && !endsLink(text[runBegin - 1]))
        --runBegin;
    size_t runEnd = offset;
    while (runEnd < text.size() && !endsLink(text[runEnd]))
        ++runEnd;
    const std::string_view run = text.substr(runBegin, runEnd - runBegin);

    // Every start of a link in the run, nearest before the offset wins.
    for (size_t at = run.size(); at-- > 0;) {
        size_t begin = std::string_view::npos;
        std::string prefix;
        if (run.substr(at).starts_with("://") && at > 0) {
            size_t scheme = at;
            while (scheme > 0 && isSchemeCharacter(run[scheme - 1]))
                --scheme;
            while (scheme < at && !strings::isAsciiAlpha(run[scheme]))
                ++scheme;
            if (scheme < at && at + 3 < run.size())
                begin = scheme;
        } else if ((at == 0 || !isSchemeCharacter(run[at - 1]))
                   && strings::startsWith(run.substr(at), "www.", strings::CaseSensitivity::Insensitive)
                   && at + 4 < run.size()) {
            begin = at;
            prefix = "http://";
        }
        if (begin == std::string_view::npos || runBegin + begin > offset)
            continue;
        const size_t end = runBegin + trimEnd(run, begin, run.size());
        if (offset >= end)
            return {};
        Link link{runBegin + begin, end, std::string()};
        link.url = prefix + std::string(text.substr(link.begin, link.end - link.begin));
        return link;
    }
    return {};
}

static Link mailLink(std::string_view text, size_t offset)
{
    size_t begin = offset;
    while (begin > 0 && (isAddressCharacter(text[begin - 1]) || text[begin - 1] == '@'))
        --begin;
    size_t end = offset;
    while (end < text.size() && (isAddressCharacter(text[end]) || text[end] == '@'))
        ++end;
    end = trimEnd(text, begin, end);
    const std::string_view address = text.substr(begin, end - begin);
    const size_t at = address.find('@');
    if (at == std::string_view::npos || at == 0 || address.find('@', at + 1) != std::string_view::npos)
        return {};
    const std::string_view domain = address.substr(at + 1);
    const size_t dot = domain.rfind('.');
    if (dot == std::string_view::npos || dot == 0 || domain.size() - dot < 3 || offset >= end)
        return {};
    for (size_t i = dot + 1; i < domain.size(); ++i) {
        if (!strings::isAsciiAlpha(domain[i]))
            return {};
    }
    return Link{begin, end, "mailto:" + std::string(address)};
}

Link linkAt(std::string_view text, size_t offset)
{
    if (offset >= text.size() || endsLink(text[offset]))
        return {};
    if (Link link = webLink(text, offset); link.isValid())
        return link;
    return mailLink(text, offset);
}

}
