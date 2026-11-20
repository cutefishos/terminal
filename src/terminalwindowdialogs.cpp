// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include "terminalwindow.h"

#include "base/event_loop.h"
#include "base/i18n.h"
#include "base/strings.h"
#include "ui/about_dialog.h"
#include "ui/dialog_window.h"
#include "ui/window.h"

#include "processhelper.h"
#include "settingsdialog.h"

using namespace cutefish::ui;
using cutefish::i18n::tr;
using cutefish::i18n::trn;
namespace strings = cutefish::base::strings;

void TerminalWindow::showSettings()
{
    if (!m_settingsDialog)
        m_settingsDialog = std::make_unique<SettingsDialog>(this);
    m_settingsDialog->show();
}

void TerminalWindow::showAbout()
{
    if (m_aboutDialog) {
        m_aboutDialog->show();
        return;
    }

    m_aboutDialog = std::make_unique<AboutDialog>(m_window.get());
    m_aboutDialog->setName(tr("Terminal"));
    m_aboutDialog->setDescription(tr("Runs a shell and the programs started in it."));
    m_aboutDialog->setIconSource("utilities-terminal");
    m_aboutDialog->onClosed = [this] {
        std::shared_ptr<AboutDialog> doomed(std::move(m_aboutDialog));
        cutefish::base::EventLoop::current()->post([doomed] {});
    };
    m_aboutDialog->show();
}

// A link in the output can name any scheme, and opening one starts whatever
// program claims it; only web and mail links open without asking.
void TerminalWindow::openLink(const std::string& url)
{
    const std::string lower = strings::toAsciiLower(url);
    if (lower.starts_with("http:") || lower.starts_with("https:") || lower.starts_with("mailto:")) {
        ProcessHelper::openUrl(url);
        return;
    }

    m_pendingLink = url;
    showLinkConfirm();
}

void TerminalWindow::showLinkConfirm()
{
    if (!m_linkConfirm) {
        m_linkConfirm = std::make_unique<ConfirmDialog>(m_window.get(), tr("Open this link?"));
        m_linkConfirm->setConfirmText(tr("Open"));
        m_linkConfirm->onAccepted = [this] { ProcessHelper::openUrl(m_pendingLink); };
    }
    const std::u16string utf16 = strings::utf8ToUtf16(m_pendingLink);
    m_linkConfirm->setText(utf16.size() > 300 ? strings::utf16ToUtf8(utf16.substr(0, 299)) + "\xe2\x80\xa6"
                                              : m_pendingLink);
    m_linkConfirm->show();
}

// Line breaks in a paste run as if Enter were pressed, so a paste with any
// waits here first.
void TerminalWindow::confirmPaste(const std::string& text, std::function<void()> action)
{
    std::string trimmed = text;
    while (!trimmed.empty() && trimmed.back() == '\n')
        trimmed.pop_back();
    const std::vector<std::string> lines = strings::split(trimmed, '\n');
    std::vector<std::string> shown;
    for (size_t i = 0; i < lines.size() && i < 4; ++i) {
        const std::u16string line = strings::utf8ToUtf16(lines[i]);
        shown.push_back(line.size() > 60 ? strings::utf16ToUtf8(line.substr(0, 59)) + "\xe2\x80\xa6" : lines[i]);
    }
    if (lines.size() > 4)
        shown.push_back("\xe2\x80\xa6");

    m_pasteLineCount = lines.size();
    m_pastePreview = strings::join(shown, "\n");
    m_pendingPaste = std::move(action);
    showPasteConfirm();
}

void TerminalWindow::showPasteConfirm()
{
    if (!m_pasteConfirm) {
        m_pasteConfirm = std::make_unique<ConfirmDialog>(m_window.get(), std::string());
        m_pasteConfirm->setConfirmText(tr("Paste"));
        m_pasteConfirm->onAccepted = [this] {
            if (auto pending = std::exchange(m_pendingPaste, nullptr))
                pending();
        };
        m_pasteConfirm->onRejected = [this] { m_pendingPaste = nullptr; };
    }

    m_pasteConfirm->setTitle(m_pasteLineCount > 1 ? trn("Paste {n} line?", "Paste {n} lines?", m_pasteLineCount)
                                                  : std::string(tr("Paste this line and press Enter?")));
    m_pasteConfirm->setText(m_pastePreview);
    m_pasteConfirm->show();
}
