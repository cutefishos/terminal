// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include "links.h"

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include <algorithm>
#include <memory>

#include "base/strings.h"

namespace strings = cutefish::base::strings;

namespace term {

// URLs with a scheme, absolute and dot-relative paths, and bare relative
// paths with a dot in them, matched as the reference terminal's default
// link pattern does; see the README.
static const std::string& linkPattern()
{
    static const std::string pattern = [] {
        const std::string schemes =
                R"(https?://|mailto:|ftp://|file:|ssh:|git://|ssh://|tel:|magnet:|ipfs://|ipns://|gemini://|gopher://|news:)";
        const std::string ipv6 = R"((?:\[[:0-9a-fA-F]+(?:[:0-9a-fA-F]*)+\](?::[0-9]+)?))";
        const std::string schemeChars = R"([\w\-.~:/?#@!$&*+,;=%])";
        const std::string pathChars = R"([\w\-.~:\/?#@!$&*+;=%])";
        const std::string bracketedWordSuffix = R"((?:[\(\[]\w*[\)\]])?)";
        const std::string noTrailingPunctuation = R"((?<![,.]))";
        const std::string noTrailingColon = R"((?<!:))";
        const std::string dottedPathLookahead = R"((?=[\w\-.~:\/?#@!$&*+;=%]*\.))";
        const std::string nonDottedPathLookahead = R"((?![\w\-.~:\/?#@!$&*+;=%]*\.))";
        const std::string dottedPathSpaceSegments =
                R"((?:(?<!:) (?!\w+:\/\/)(?!\.{0,2}\/)(?!~\/)[\w\-.~:\/?#@!$&*+;=%]*[\/.])*)";
        const std::string anyPathSpaceSegments =
                R"((?:(?<!:) (?!\w+:\/\/)(?!\.{0,2}\/)(?!~\/)[\w\-.~:\/?#@!$&*+;=%]+)*)";

        const std::string schemeUrl = "(?:" + schemes + ")(?:" + ipv6 + "|" + schemeChars + "+" + bracketedWordSuffix
                + ")+" + noTrailingPunctuation;
        const std::string rootedOrRelativePrefix =
                R"((?:\.\.\/|\.\/|(?<!\w)~\/|(?:[\w][\w\-.]*\/)*(?<!\w)\$[A-Za-z_]\w*\/|\.[\w][\w\-.]*\/|(?<![\w~\/])\/(?!\/)))";
        const std::string rootedOrRelativePath = rootedOrRelativePrefix + "(?:" + dottedPathLookahead + pathChars + "+"
                + dottedPathSpaceSegments + noTrailingColon + "|" + nonDottedPathLookahead + pathChars + "+"
                + anyPathSpaceSegments + noTrailingColon + ")";
        // The reference's (?<!\$\d*) is unbounded, which PCRE2 cannot look
        // behind for; after a digit (?<!\w) already fails, so only "$" itself is left.
        const std::string bareRelativePrefix = R"((?<!\$)(?<!\w)[\w][\w\-.]*\/)";
        const std::string bareRelativePath = dottedPathLookahead + bareRelativePrefix + pathChars + "+" + noTrailingColon;
        return schemeUrl + "|" + rootedOrRelativePath + "|" + bareRelativePath;
    }();
    return pattern;
}

struct CodeDeleter
{
    void operator()(pcre2_code* code) const { pcre2_code_free(code); }
};

static const pcre2_code* linkRegex()
{
    static const std::unique_ptr<pcre2_code, CodeDeleter> code = [] {
        int error = 0;
        PCRE2_SIZE offset = 0;
        const std::string& pattern = linkPattern();
        // \w and the classes match letters of any script, as the pattern expects.
        return std::unique_ptr<pcre2_code, CodeDeleter>(
                pcre2_compile(reinterpret_cast<PCRE2_SPTR>(pattern.data()), pattern.size(),
                              PCRE2_UTF | PCRE2_UCP | PCRE2_MATCH_INVALID_UTF, &error, &offset, nullptr));
    }();
    return code.get();
}

std::vector<std::pair<size_t, size_t>> linkMatches(std::string_view text)
{
    std::vector<std::pair<size_t, size_t>> matches;
    const pcre2_code* code = linkRegex();
    if (!code)
        return matches;
    pcre2_match_data* data = pcre2_match_data_create_from_pattern(code, nullptr);
    const auto subject = reinterpret_cast<PCRE2_SPTR>(text.data());
    size_t start = 0;
    while (start <= text.size()
           && pcre2_match(code, subject, text.size(), start, 0, data, nullptr) > 0) {
        const PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(data);
        if (ovector[1] == ovector[0]) {
            ++start;
            continue;
        }
        matches.emplace_back(ovector[0], ovector[1]);
        start = ovector[1];
    }
    pcre2_match_data_free(data);
    return matches;
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

// What ends a link written without a scheme: space, and the quotes and
// brackets text puts around one.
static bool endsLink(char c)
{
    return static_cast<unsigned char>(c) <= ' ' || c == '"' || c == '\'' || c == '<' || c == '>' || c == '`'
            || c == 0x7f;
}

// Punctuation after a link belongs to the sentence.
static size_t trimEnd(std::string_view text, size_t begin, size_t end)
{
    while (end > begin && std::string_view(".,:;!?").find(text[end - 1]) != std::string_view::npos)
        --end;
    return end;
}

// "www." with no scheme, opened over http.
static Link webLink(std::string_view text, size_t offset)
{
    size_t begin = offset;
    while (begin > 0 && !endsLink(text[begin - 1]))
        --begin;
    for (size_t at = offset + 1; at-- > begin;) {
        if ((at == 0 || !isSchemeCharacter(text[at - 1]))
            && strings::startsWith(text.substr(at), "www.", strings::CaseSensitivity::Insensitive)) {
            size_t end = at;
            while (end < text.size() && !endsLink(text[end]))
                ++end;
            end = trimEnd(text, at, end);
            if (end <= at + 4 || offset >= end)
                return {};
            return Link{at, end, "http://" + std::string(text.substr(at, end - at)), false};
        }
    }
    return {};
}

// A mail address with no scheme, opened as mailto:.
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
    return Link{begin, end, "mailto:" + std::string(address), false};
}

static bool hasScheme(std::string_view text)
{
    const size_t colon = text.find(':');
    if (colon == std::string_view::npos || colon == 0)
        return false;
    return std::all_of(text.begin(), text.begin() + colon, isSchemeCharacter);
}

Link linkAt(std::string_view text, size_t offset)
{
    if (offset >= text.size())
        return {};
    for (const auto& [begin, end] : linkMatches(text)) {
        if (offset < begin || offset >= end)
            continue;
        const std::string_view written = text.substr(begin, end - begin);
        return Link{begin, end, std::string(written), !hasScheme(written)};
    }
    if (endsLink(text[offset]))
        return {};
    if (Link link = webLink(text, offset); link.isValid())
        return link;
    return mailLink(text, offset);
}

}
