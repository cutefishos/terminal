// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "base/signal.h"
#include "base/timer.h"
#include "ui/view.h"

namespace Konsole {
class TerminalSession;
class TerminalView;
}

namespace cutefish::ui {
class Menu;
class ScrollBar;
}

class FindBar;
class TerminalWindow;

// One tab: a shell on a pty, its terminal view with the scroll bar over its
// right margin, the find bar and the context menu.
class TerminalPage : public cutefish::ui::View
{
public:
    // `path` is where the shell starts ("$PWD", "$HOME" or a directory),
    // `command` a program and its arguments to run instead.
    TerminalPage(TerminalWindow* window, const std::string& path, const std::vector<std::string>& command);
    ~TerminalPage() override;

    Konsole::TerminalSession& session() const { return *m_session; }
    Konsole::TerminalView* terminal() const { return m_terminal; }

    // The tab is in this window now, moved there with its shell.
    void setTerminalWindow(TerminalWindow* window);
    TerminalWindow* terminalWindow() const { return m_window; }

    // Set by renaming the tab; empty follows the session.
    void setCustomTitle(const std::string& title);
    std::string title() const;

    // The settings and the window's zoom and colours, applied again.
    void applySettings();

    void startShell();
    void openFind();
    void focusTerminal();
    // A drop: paths and links as one shell word each, text pasted.
    void dropUrls(const std::vector<std::string>& urls);

    cutefish::base::Signal<> titleChanged;
    // The shell ended.
    cutefish::base::Signal<> terminalClosed;

protected:
    void layout() override;
    bool canDrop(const cutefish::ui::DropOffer& offer) const override;
    uint32_t dragUpdate(const cutefish::ui::DragEvent& event) override;
    uint32_t drop(const cutefish::ui::DragEvent& event) override;

private:
    bool handleShortcut(const cutefish::ui::KeyEvent& event);
    void openMenu(double x, double y);
    void updateScrollBar();

    TerminalWindow* m_window;
    std::string m_customTitle;
    std::vector<std::string> m_command;
    std::unique_ptr<Konsole::TerminalSession> m_session;
    Konsole::TerminalView* m_terminal;
    cutefish::ui::ScrollBar* m_scrollBar;
    FindBar* m_findBar = nullptr;
    std::unique_ptr<cutefish::ui::Menu> m_menu;
    cutefish::base::Timer m_scrollActivity;
    std::vector<cutefish::base::Subscription> m_connections;

    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);

    // What the menu was opened over.
    std::string m_menuLink;
    std::string m_menuPath;
    bool m_menuCanPaste = false;
};
