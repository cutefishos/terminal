// SPDX-FileCopyrightText: 2024 Mitchell Hashimoto, Ghostty contributors
// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <string>
#include <utility>

#include "links.h"

using term::linkAt;

static std::string urlAt(const std::string& text, const std::string& under)
{
    return linkAt(text, text.find(under)).url;
}

// The reference pattern's own cases, each input with its first match; see the README.
static const std::pair<const char*, const char*> kMatches[] = {
    {"hello https://example.com world", "https://example.com"},
    {"https://example.com/foo(bar) more", "https://example.com/foo(bar)"},
    {"https://example.com/foo(bar)baz more", "https://example.com/foo(bar)baz"},
    {"Link inside (https://example.com) parens", "https://example.com"},
    {"Link period https://example.com. More text.", "https://example.com"},
    {"Link trailing comma https://example.com, more text.", "https://example.com"},
    {"Link in double quotes \"https://example.com\" and more", "https://example.com"},
    {"Link in single quotes 'https://example.com' and more", "https://example.com"},
    {"some file with https://google.com https://duckduckgo.com links.", "https://google.com"},
    {"and links in it. links https://yahoo.com mailto:test@example.com ssh://1.2.3.4", "https://yahoo.com"},
    {"also match http://example.com non-secure links", "http://example.com"},
    {"match tel://+12123456789 phone numbers", "tel://+12123456789"},
    {"match with query url https://example.com?query=1&other=2 and more text.", "https://example.com?query=1&other=2"},
    {"url with dashes [mode 2027](https://github.com/contour-terminal/terminal-unicode-core) for better unicode support", "https://github.com/contour-terminal/terminal-unicode-core"},
    {"dot.http://example.com", "http://example.com"},
    {"weird characters https://example.com/~user/?query=1&other=2#hash and more", "https://example.com/~user/?query=1&other=2#hash"},
    {"square brackets https://example.com/[foo] and more", "https://example.com/[foo]"},
    {"[13]:TooManyStatements: TempFile#assign_temp_file_to_entity has approx 7 statements [https://example.com/docs/Too-Many-Statements.md]", "https://example.com/docs/Too-Many-Statements.md"},
    {"match ftp://example.com ftp links", "ftp://example.com"},
    {"match file://example.com file links", "file://example.com"},
    {"match ssh://example.com ssh links", "ssh://example.com"},
    {"match git://example.com git links", "git://example.com"},
    {"/tmp/test.txt http://www.google.com", "/tmp/test.txt"},
    {"match tel:+18005551234 tel links", "tel:+18005551234"},
    {"match magnet:?xt=urn:btih:1234567890 magnet links", "magnet:?xt=urn:btih:1234567890"},
    {"match ipfs://QmSomeHashValue ipfs links", "ipfs://QmSomeHashValue"},
    {"match ipns://QmSomeHashValue ipns links", "ipns://QmSomeHashValue"},
    {"match gemini://example.com gemini links", "gemini://example.com"},
    {"match gopher://example.com gopher links", "gopher://example.com"},
    {"match news:comp.infosystems.www.servers.unix news links", "news:comp.infosystems.www.servers.unix"},
    {"/Users/ghostty.user/code/example.py", "/Users/ghostty.user/code/example.py"},
    {"/Users/ghostty.user/code/../example.py", "/Users/ghostty.user/code/../example.py"},
    {"/Users/ghostty.user/code/../example.py hello world", "/Users/ghostty.user/code/../example.py"},
    {"../example.py", "../example.py"},
    {"../example.py ", "../example.py"},
    {"first time ../example.py contributor ", "../example.py"},
    {"[link](/home/user/ghostty.user/example)", "/home/user/ghostty.user/example"},
    {"Serving HTTP on :: port 8000 (http://[::]:8000/)", "http://[::]:8000/"},
    {"IPv6 address https://[2001:db8::1]:8080/path", "https://[2001:db8::1]:8080/path"},
    {"IPv6 localhost http://[::1]:3000", "http://[::1]:3000"},
    {"Complex IPv6 https://[2001:db8:85a3:8d3:1319:8a2e:370:7348]:443/", "https://[2001:db8:85a3:8d3:1319:8a2e:370:7348]:443/"},
    {"IPv6 with path https://[2001:db8::1]/path/to/resource", "https://[2001:db8::1]/path/to/resource"},
    {"IPv6 with query https://[2001:db8::1]:8080/api?param=value&other=123", "https://[2001:db8::1]:8080/api?param=value&other=123"},
    {"IPv6 compressed http://[2001:db8::]:80/", "http://[2001:db8::]:80/"},
    {"IPv6 multiple zeros http://[2001:0:0:0:0:0:0:1]", "http://[2001:0:0:0:0:0:0:1]"},
    {"IPv6 link-local https://[fe80::1234:5678:9abc]", "https://[fe80::1234:5678:9abc]"},
    {"IPv6 multicast http://[ff02::1]/stream", "http://[ff02::1]/stream"},
    {"IPv6 in markdown [link](http://[2001:db8::1]/docs)", "http://[2001:db8::1]/docs"},
    {"./spaces-end.   ", "./spaces-end."},
    {"./space middle", "./space middle"},
    {"../test folder/file.txt", "../test folder/file.txt"},
    {"/tmp/test folder/file.txt", "/tmp/test folder/file.txt"},
    {"/tmp/test  folder/file.txt", "/tmp/test"},
    {"diff --git a/src/font/shaper/harfbuzz.zig b/src/font/shaper/harfbuzz.zig", "a/src/font/shaper/harfbuzz.zig"},
    {"/tmp/foo /tmp/bar", "/tmp/foo"},
    {"/tmp/foo.txt /tmp/bar.txt", "/tmp/foo.txt"},
    {"src/config/url.zig", "src/config/url.zig"},
    {"app/folder/file.rb:1", "app/folder/file.rb:1"},
    {"modified:   src/config/url.zig", "src/config/url.zig"},
    {"lib/ghostty/terminal.zig:42:10", "lib/ghostty/terminal.zig:42:10"},
    {"some-pkg/src/file.txt more text", "some-pkg/src/file.txt"},
    {"src/foo.c,baz.txt", "src/foo.c"},
    {"~/foo/bar.txt", "~/foo/bar.txt"},
    {"open ~/Documents/notes.md please", "~/Documents/notes.md"},
    {"~/.config/ghostty/config", "~/.config/ghostty/config"},
    {"directory: ~/src/ghostty-org/ghostty", "~/src/ghostty-org/ghostty"},
    {"$HOME/src/config/url.zig", "$HOME/src/config/url.zig"},
    {"project dir: $PWD/src/ghostty/main.zig", "$PWD/src/ghostty/main.zig"},
    {"foo/$BAR/baz", "foo/$BAR/baz"},
    {".foo/bar/$VAR", ".foo/bar/$VAR"},
    {".config/ghostty/config", ".config/ghostty/config"},
    {"loaded from .local/share/ghostty/state.db now", ".local/share/ghostty/state.db"},
    {"../some/where", "../some/where"},
    {"  - shared/src/foo/SomeItem.m:12, shared/src/", "shared/src/foo/SomeItem.m:12"},
    {"foo.local/share", "foo.local/share"},
    {"2024/report.txt", "2024/report.txt"},
    {"./foo bar,baz", "./foo bar"},
    {"/tmp/foo bar,baz", "/tmp/foo bar"},
    {"./.config/ghostty: Needs upstream (main)", "./.config/ghostty"},
    {"./Downloads: Operation not permitted", "./Downloads"},
};

static const char* const kNoMatches[] = {
    "input/output",
    "foo/bar",
    "$10/bar",
    "$10/$20",
    "$10/bar.txt",
    "foo/bar,baz.txt",
    "foo$BAR/baz.txt",
    "foo~/bar.txt",
    "// foo bar",
    "//foo",
};

TEST(LinksTest, TheReferenceCasesMatchAsTheyDoThere)
{
    for (const auto& [input, expected] : kMatches) {
        const auto matches = term::linkMatches(input);
        ASSERT_FALSE(matches.empty()) << input;
        const std::string text(input);
        EXPECT_EQ(text.substr(matches[0].first, matches[0].second - matches[0].first), expected) << input;
    }
    for (const char* input : kNoMatches)
        EXPECT_TRUE(term::linkMatches(input).empty()) << input;
}

TEST(LinksTest, WebLinksOpenAsWritten)
{
    EXPECT_EQ(urlAt("see https://example.com/a?b=c#d now", "example"), "https://example.com/a?b=c#d");
    EXPECT_EQ(urlAt("ftp://host/file", "file"), "ftp://host/file");
    EXPECT_EQ(urlAt("tel:+15551234 call", "555"), "tel:+15551234");
    EXPECT_EQ(urlAt("go to www.example.org", "example"), "http://www.example.org");
    EXPECT_EQ(urlAt("no link here", "link"), "");
}

TEST(LinksTest, PathsAreLinksToOpenFromTheShellsDirectory)
{
    const std::string text = "edit src/config/url.zig and /etc/hosts.conf now";
    const term::Link relative = linkAt(text, text.find("config"));
    EXPECT_EQ(relative.url, "src/config/url.zig");
    EXPECT_TRUE(relative.path);
    const term::Link absolute = linkAt(text, text.find("hosts"));
    EXPECT_EQ(absolute.url, "/etc/hosts.conf");
    EXPECT_TRUE(absolute.path);
    EXPECT_FALSE(linkAt("see https://example.com", 6).path);
}

TEST(LinksTest, SentencePunctuationIsNotPartOfTheLink)
{
    EXPECT_EQ(urlAt("read https://example.com/page.", "page"), "https://example.com/page");
    EXPECT_EQ(urlAt("(see https://example.com/x)", "example"), "https://example.com/x");
    EXPECT_EQ(urlAt("https://en.wikipedia.org/wiki/C_(language)", "wiki"),
              "https://en.wikipedia.org/wiki/C_(language)");
    EXPECT_EQ(urlAt("<https://example.com/y>", "example"), "https://example.com/y");
    EXPECT_EQ(urlAt("\"https://example.com/z\",", "example"), "https://example.com/z");
}

TEST(LinksTest, OnlyTheCoveredTextCounts)
{
    const std::string text = "https://example.com/a and more";
    EXPECT_FALSE(linkAt(text, text.find("and")).isValid());
    const term::Link link = linkAt(text, 3);
    EXPECT_EQ(link.begin, 0u);
    EXPECT_EQ(link.end, 21u);
}

TEST(LinksTest, MailAddressesBecomeMailto)
{
    EXPECT_EQ(urlAt("mail me@example.com today", "example"), "mailto:me@example.com");
    EXPECT_EQ(urlAt("write mailto:me@example.com today", "example"), "mailto:me@example.com");
    EXPECT_EQ(urlAt("user@localhost", "user"), "");
    EXPECT_EQ(urlAt("a@b@c.com", "b"), "");
}
