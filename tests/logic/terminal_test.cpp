// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <string>
#include <utility>

#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "colorschemes.h"
#include "search.h"
#include "terminal.h"

using namespace term;

// Stands in for the pty: program output goes in, and what the terminal
// answers or what a key types comes out.
class Pty
{
public:
    explicit Pty(int columns = 80, int lines = 24)
        : m_terminal(columns, lines)
    {
        m_terminal.onSend = [this](std::string_view bytes) { m_sent.append(bytes); };
    }

    Terminal& terminal() { return m_terminal; }

    std::string print(const std::string& output)
    {
        m_terminal.write(output);
        return std::exchange(m_sent, {});
    }

    std::string press(uint32_t keysym, uint32_t modifiers = 0, const std::string& text = {}, uint32_t keycode = 0)
    {
        KeyInput key;
        key.keysym = keysym;
        key.keycode = keycode;
        key.modifiers = modifiers;
        key.text = text;
        m_terminal.sendKey(key);
        return std::exchange(m_sent, {});
    }

    std::string paste(const std::string& text)
    {
        m_terminal.paste(text);
        return std::exchange(m_sent, {});
    }

    std::string sent() { return std::exchange(m_sent, {}); }

    // The text of a line of the screen, without the trailing blanks.
    std::string line(int index)
    {
        Terminal::LogicalLine text = m_terminal.logicalLine(index);
        std::string result;
        for (size_t i = 0; i < text.text.size(); ++i) {
            if (text.cells[i].first == index)
                result += text.text[i];
        }
        while (!result.empty() && result.back() == ' ')
            result.pop_back();
        return result;
    }

private:
    Terminal m_terminal;
    std::string m_sent;
};

TEST(TerminalTest, KittyKeyboardQueryIsAnswered)
{
    Pty pty;
    // What a program asks to learn whether Shift+Enter can reach it.
    EXPECT_EQ(pty.print("\033[?u"), "\033[?0u");
    pty.print("\033[>1u");
    EXPECT_EQ(pty.print("\033[?u"), "\033[?1u");
    pty.print("\033[<u");
    EXPECT_EQ(pty.print("\033[?u"), "\033[?0u");
}

TEST(TerminalTest, EachScreenKeepsItsOwnKeyFlags)
{
    Pty pty;
    pty.print("\033[>1u\033[?1049h");
    EXPECT_EQ(pty.print("\033[?u"), "\033[?0u");
    pty.print("\033[?1049l");
    EXPECT_EQ(pty.print("\033[?u"), "\033[?1u");
}

TEST(TerminalTest, ShiftEnterIsItsOwnKeyOnceAProgramAsks)
{
    Pty pty;
    EXPECT_EQ(pty.press(XKB_KEY_Return, 0, "\r"), "\r");
    pty.print("\033[>1u");
    EXPECT_EQ(pty.press(XKB_KEY_Return, ShiftKey, "\r"), "\033[13;2u");
    EXPECT_EQ(pty.press(XKB_KEY_Return, 0, "\r"), "\r");
}

struct KeyCase
{
    const char* name;
    uint32_t keysym;
    uint32_t modifiers;
    std::string text;
    std::string sent;
    uint32_t keycode = 0;
};

TEST(TerminalTest, KeysWithoutTheKittyProtocol)
{
    const KeyCase cases[] = {
        {"a", 'a', 0, "a", "a"},
        {"shift+a", 'a', ShiftKey, "A", "A"},
        {"ctrl+c", 'c', ControlKey, "", "\x03"},
        {"alt+b", 'b', AltKey, "b", "\033b"},
        {"tab", XKB_KEY_Tab, 0, "\t", "\t"},
        {"shift+tab", XKB_KEY_Tab, ShiftKey, "", "\033[Z"},
        {"backspace", XKB_KEY_BackSpace, 0, "\b", "\x7f"},
        {"escape", XKB_KEY_Escape, 0, "\033", "\033"},
        {"up", XKB_KEY_Up, 0, "", "\033[A"},
        {"ctrl+up", XKB_KEY_Up, ControlKey, "", "\033[1;5A"},
        {"f1", XKB_KEY_F1, 0, "", "\033OP"},
        {"delete", XKB_KEY_Delete, 0, "", "\033[3~"},
        {"page down", XKB_KEY_Page_Down, 0, "", "\033[6~"},
        {"accented", XKB_KEY_eacute, 0, "\xc3\xa9", "\xc3\xa9"},
        {"ctrl+cyrillic", XKB_KEY_Cyrillic_ef, ControlKey, "", "\x01", KEY_A},
    };
    for (const KeyCase& key : cases) {
        Pty pty;
        EXPECT_EQ(pty.press(key.keysym, key.modifiers, key.text, key.keycode), key.sent) << key.name;
    }
}

TEST(TerminalTest, KeysAreDisambiguated)
{
    const KeyCase cases[] = {
        {"enter", XKB_KEY_Return, 0, "\r", "\r"},
        {"ctrl+enter", XKB_KEY_Return, ControlKey, "\r", "\033[13;5u"},
        {"shift+tab", XKB_KEY_Tab, ShiftKey, "", "\033[9;2u"},
        {"escape", XKB_KEY_Escape, 0, "\033", "\033[27u"},
        {"a", 'a', 0, "a", "a"},
        {"ctrl+a", 'a', ControlKey, "", "\033[97;5u"},
        {"ctrl+shift+c", 'c', ControlKey | ShiftKey, "", "\033[99;6u"},
        {"alt+b", 'b', AltKey, "b", "\033[98;3u"},
        {"ctrl+space", ' ', ControlKey, " ", "\033[32;5u"},
        {"ctrl+cyrillic", XKB_KEY_Cyrillic_ef, ControlKey, "", "\033[1092;5u", KEY_A},
        {"up", XKB_KEY_Up, 0, "", "\033[A"},
        {"f13", XKB_KEY_F13, 0, "", "\033[57376u"},
    };
    for (const KeyCase& key : cases) {
        Pty pty;
        pty.print("\033[>1u");
        EXPECT_EQ(pty.press(key.keysym, key.modifiers, key.text, key.keycode), key.sent) << key.name;
    }
}

TEST(TerminalTest, CursorKeysFollowApplicationMode)
{
    Pty pty;
    pty.print("\033[?1h");
    EXPECT_EQ(pty.press(XKB_KEY_Up), "\033OA");
    pty.print("\033[?1l");
    EXPECT_EQ(pty.press(XKB_KEY_Up), "\033[A");
}

TEST(TerminalTest, PasteFollowsBracketedMode)
{
    Pty pty;
    EXPECT_EQ(pty.paste("ls -l\nwc"), "ls -l\rwc");
    pty.print("\033[?2004h");
    EXPECT_TRUE(pty.terminal().bracketedPaste());
    EXPECT_EQ(pty.paste("ls -l\nwc"), "\033[200~ls -l\nwc\033[201~");
}

TEST(TerminalTest, PasteCannotEndTheBracketEarly)
{
    Pty pty;
    pty.print("\033[?2004h");
    const std::string sent = pty.paste("echo \033[201~touch pwned");
    EXPECT_TRUE(sent.starts_with("\033[200~"));
    EXPECT_TRUE(sent.ends_with("\033[201~"));
    EXPECT_EQ(sent.find("\033[201~"), sent.size() - 6) << sent;
}

TEST(TerminalTest, RepliesToTheQueriesProgramsSend)
{
    Pty pty;
    EXPECT_EQ(pty.print("\033[>q"), "\033P>|cutefish-terminal 1.0\033\\");
    EXPECT_EQ(pty.print("\033[c"), "\033[?62;22c");
    EXPECT_EQ(pty.print("\033[6n"), "\033[1;1R");
    // Light or dark, from the scheme's background.
    pty.terminal().setColorScheme(colorScheme("TokyoNight-Day"));
    EXPECT_EQ(pty.print("\033[?996n"), "\033[?997;2n");
    pty.terminal().setColorScheme(colorScheme("TokyoNight"));
    EXPECT_EQ(pty.print("\033[?996n"), "\033[?997;1n");
}

TEST(TerminalTest, ImageQueriesGetNoAnswer)
{
    Pty pty;
    EXPECT_EQ(pty.print("\033_Gi=31,s=1,v=1,a=q,t=d,f=24;AAAA\033\\"), "");
    EXPECT_EQ(pty.print("x"), "");
    EXPECT_EQ(pty.line(0), "x");
}

TEST(TerminalTest, TitleAndBellArriveAfterTheWrite)
{
    Pty pty;
    int titles = 0;
    int bells = 0;
    pty.terminal().onTitleChanged = [&] { ++titles; };
    pty.terminal().onBell = [&] { ++bells; };
    pty.print("\033]2;one\007\033]0;two\033\\\007\007");
    EXPECT_EQ(titles, 1);
    EXPECT_EQ(bells, 1);
    EXPECT_EQ(pty.terminal().title(), "two");
}

TEST(TerminalTest, ScrollbackKeepsTheLinesAskedFor)
{
    Pty pty(20, 5);
    pty.terminal().setScrollbackLines(1000);
    for (int i = 0; i < 300; ++i)
        pty.print(std::to_string(i) + "\r\n");
    EXPECT_GE(pty.terminal().scrollbackLines(), 296u);

    // A limit is kept in whole pages, so it holds at least what was asked.
    Pty small(20, 5);
    small.terminal().setScrollbackLines(10);
    for (int i = 0; i < 5000; ++i)
        small.print(std::to_string(i) + "\r\n");
    EXPECT_GE(small.terminal().scrollbackLines(), 10u);
    EXPECT_LT(small.terminal().scrollbackLines(), 5000u);
}

TEST(TerminalTest, ClearingKeepsTheCursorLine)
{
    Pty pty(20, 5);
    pty.print("a\r\nb\r\nc\r\nd\r\ne\r\nf\r\nprompt$ ");
    EXPECT_GT(pty.terminal().scrollbackLines(), 0u);
    pty.terminal().clearKeepingCursorLine();
    EXPECT_EQ(pty.terminal().scrollbackLines(), 0u);
    EXPECT_EQ(pty.terminal().cursorLine(), 0);
    EXPECT_EQ(pty.terminal().cursorColumn(), 8);
    EXPECT_EQ(pty.line(0), "prompt$");
    EXPECT_EQ(pty.line(1), "");
    // A sequence left unfinished does not take the clear in.
    pty.print("\033[3");
    pty.terminal().clearScreen();
    EXPECT_EQ(pty.line(0), "");
    pty.print("x");
    EXPECT_EQ(pty.line(0), "x");
}

TEST(TerminalTest, SelectionCopiesUnwrappedText)
{
    Pty pty(10, 4);
    pty.print("hello world, wrapped\r\nnext");
    EXPECT_FALSE(pty.terminal().hasSelection());
    EXPECT_TRUE(pty.terminal().selectWord(1, 0));
    EXPECT_EQ(pty.terminal().selectedText(), "hello");
    pty.terminal().selectAll();
    EXPECT_EQ(pty.terminal().selectedText(), "hello world, wrapped\nnext");
    pty.terminal().clearSelection();
    EXPECT_FALSE(pty.terminal().hasSelection());
}

TEST(TerminalTest, LogicalLinesJoinWrappedRows)
{
    Pty pty(10, 4);
    pty.print("see https://example.com/x now");
    const Terminal::LogicalLine line = pty.terminal().logicalLine(1);
    EXPECT_TRUE(line.text.starts_with("see https://example.com/x now")) << line.text;
    EXPECT_EQ(line.cells[4], std::make_pair(0, 4));
    EXPECT_EQ(line.cells[10], std::make_pair(1, 0));
}

TEST(TerminalTest, HyperlinksAreRead)
{
    Pty pty;
    pty.print("\033]8;;https://example.com/\033\\link\033]8;;\033\\ plain");
    EXPECT_EQ(pty.terminal().hyperlinkAt(1, 0), "https://example.com/");
    EXPECT_EQ(pty.terminal().hyperlinkAt(6, 0), "");
}

TEST(TerminalTest, SearchHonoursCaseWhenAsked)
{
    Pty pty(40, 6);
    pty.print("Needle one\r\nneedle two\r\nNEEDLE three\r\n");
    Search search(pty.terminal());
    // Newest first, then towards older output.
    ASSERT_TRUE(search.find("needle", false, false));
    EXPECT_EQ(pty.terminal().selectedText(), "NEEDLE");
    ASSERT_TRUE(search.find("needle", false, false));
    EXPECT_EQ(pty.terminal().selectedText(), "needle");

    Search exact(pty.terminal());
    ASSERT_TRUE(exact.find("Needle", false, true));
    EXPECT_EQ(pty.terminal().selectedText(), "Needle");
    EXPECT_FALSE(exact.find("NEEDLe", false, true));
    EXPECT_FALSE(pty.terminal().hasSelection());
}

static uint32_t background(Terminal& terminal)
{
    GhosttyColorRgb rgb{};
    ghostty_terminal_get(terminal.handle(), GHOSTTY_TERMINAL_DATA_COLOR_BACKGROUND, &rgb);
    return uint32_t(rgb.r) << 16 | uint32_t(rgb.g) << 8 | rgb.b;
}

TEST(TerminalTest, ResetsKeepTheColorScheme)
{
    Pty pty;
    pty.terminal().setColorScheme(colorScheme("Dracula"));
    pty.print("\033]11;#ff0000\033\\");
    EXPECT_EQ(background(pty.terminal()), 0xff0000u);
    pty.terminal().reset();
    EXPECT_EQ(background(pty.terminal()), 0x21222cu);
}

TEST(TerminalTest, ColorSchemesFallBackToTheDefault)
{
    EXPECT_EQ(colorScheme("Dracula").label, "Dracula");
    EXPECT_EQ(colorScheme("").name, "TokyoNight");
    EXPECT_EQ(colorScheme("Linux").name, "TokyoNight");
    EXPECT_EQ(colorSchemes().size(), 13u);
}

TEST(TerminalTest, TheShellsReportedDirectoryIsAPath)
{
    Pty pty;
    EXPECT_EQ(pty.terminal().reportedDirectory(), "");
    pty.print("\033]7;file://host/tmp/a%20b\a");
    EXPECT_EQ(pty.terminal().reportedDirectory(), "/tmp/a b");
    pty.print("\033]7;kitty-shell-cwd://host/tmp/c d\a");
    EXPECT_EQ(pty.terminal().reportedDirectory(), "/tmp/c d");
}
