// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <string>

#include "links.h"

using term::linkAt;

static std::string urlAt(const std::string& text, const std::string& under)
{
    return linkAt(text, text.find(under)).url;
}

TEST(LinksTest, WebLinksOpenAsWritten)
{
    EXPECT_EQ(urlAt("see https://example.com/a?b=c#d now", "example"), "https://example.com/a?b=c#d");
    EXPECT_EQ(urlAt("ftp://host/file", "file"), "ftp://host/file");
    EXPECT_EQ(urlAt("go to www.example.org", "example"), "http://www.example.org");
    EXPECT_EQ(urlAt("no link here", "link"), "");
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
    EXPECT_EQ(urlAt("user@localhost", "user"), "");
    EXPECT_EQ(urlAt("a@b@c.com", "b"), "");
}
