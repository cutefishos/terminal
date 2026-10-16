// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "base/signal.h"
#include "base/timer.h"
#include "ui/geometry.h"

namespace cutefish::ui {
class Animation;
class ConfirmDialog;
class HeaderButton;
class Label;
class Menu;
class View;
class Window;
class WindowHeader;
}

#include "terminalpage.h"

class SettingsDialog;
class TerminalSettings;
class Windows;

// A terminal window: its tabs in the title bar, each a shell.
class TerminalWindow
{
public:
    struct Options
    {
        // A tab pulled out of another window, which this window opens with
        // instead of a new shell.
        std::unique_ptr<TerminalPage> detachedPage;
        // Where the first shell starts; empty takes the command line's.
        std::string startDirectory;
        std::vector<std::string> command;
        cutefish::ui::SizeF size;
        int fontZoom = 0;
        bool restoresSize = true;
    };

    TerminalWindow(Windows& windows, TerminalSettings& settings, Options options);
    ~TerminalWindow();

    cutefish::ui::Window& window() const { return *m_window; }
    TerminalSettings& settings() const { return m_settings; }

    // The scheme's own background, so cells in the default colour and the
    // window around the text are one surface; the title bar's text and
    // buttons follow the scheme rather than the system theme.
    const std::string& colorScheme() const { return m_colorScheme; }
    cutefish::ui::Color terminalBackground() const { return m_terminalBackground; }
    cutefish::ui::Color chromeForeground() const { return m_chromeForeground; }
    bool chromeDark() const { return m_chromeDark; }
    cutefish::ui::Color schemeColor(int index, cutefish::ui::Color fallback) const;

    // Zoom is for this window only; the size in Settings stays the default.
    int fontPointSize() const;
    void zoom(int step);

    int tabCount() const { return int(m_pages.size()); }
    TerminalPage* currentPage() const;

    void openNewTab();
    void openTab(const std::string& path, const std::vector<std::string>& command = {}, int index = -1);
    void insertTab(std::unique_ptr<TerminalPage> page, int index);
    std::unique_ptr<TerminalPage> takeTab(int index);
    void closeCurrentTab();
    void closeTab(int index);
    void closeOtherTabs(int index);
    void selectTab(int index);
    void cycleTab(int step);
    void moveCurrentTab(int step);
    void moveTab(int from, int to);
    void newWindow();

    void close();
    void toggleFullScreen();
    bool isFullScreen() const;
    void flashBell();
    void showSettings();
    void openLink(const std::string& url);
    void confirmPaste(const std::string& text, std::function<void()> action);

    // The only tab of a window pulled out of another, for a window it is
    // dropped on. The empty window hides at once and goes when the drag ends.
    std::unique_ptr<TerminalPage> takePage();

private:
    class Content;
    class HeaderContent;
    class Overlay;

    void applySettings();
    void updateTitle();
    void confirmClose(const std::vector<TerminalPage*>& pages, std::function<void()> action);
    void closeProtection(int index);
    void detachTab(int index, cutefish::ui::PointF position);
    void openTabMenu(int index, cutefish::ui::PointF position);
    void attach(TerminalPage* page);
    void showPage(int index);
    int indexOfPage(const TerminalPage* page) const;
    void finishClose();

    Windows& m_windows;
    TerminalSettings& m_settings;
    std::unique_ptr<cutefish::ui::Window> m_window;
    cutefish::ui::WindowHeader* m_header = nullptr;
    HeaderContent* m_headerContent = nullptr;
    Content* m_content = nullptr;
    Overlay* m_overlay = nullptr;

    std::vector<TerminalPage*> m_pages;
    int m_currentIndex = -1;
    int m_fontZoom = 0;

    std::string m_colorScheme;
    cutefish::ui::Color m_terminalBackground;
    cutefish::ui::Color m_chromeForeground;
    bool m_chromeDark = true;
    std::vector<cutefish::ui::Color> m_schemeColors;

    // Each dialog is a window of its own, made the first time it is needed.
    std::unique_ptr<cutefish::ui::ConfirmDialog> m_exitPrompt;
    std::unique_ptr<cutefish::ui::ConfirmDialog> m_pasteConfirm;
    std::unique_ptr<cutefish::ui::ConfirmDialog> m_linkConfirm;
    std::unique_ptr<SettingsDialog> m_settingsDialog;
    std::function<void()> m_pendingAction;
    std::function<void()> m_pendingPaste;
    std::string m_pendingLink;

    // Set once the close has been confirmed, so closing does not ask again.
    bool m_quitting = false;
    // A tab of this window is being dragged out, or one of another window is
    // over the tab bar; the bar stays shown.
    bool m_detaching = false;
    bool m_incoming = false;
    std::unique_ptr<cutefish::ui::Menu> m_tabMenu;
    int m_tabMenuIndex = -1;

    std::vector<cutefish::base::Subscription> m_connections;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};
