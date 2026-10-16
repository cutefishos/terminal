// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <string>

#include "base/signal.h"
#include "ui/view.h"

namespace Konsole {
class TerminalView;
}

namespace cutefish::ui {
class HeaderButton;
}

class FindField;

// Floats over a terminal while searching its output and history. Enter goes to
// the next older match, Shift+Enter to the next newer one. Drawn in the
// terminal's colours; the field itself has no frame, only the bar does.
class FindBar : public cutefish::ui::View
{
public:
    explicit FindBar(Konsole::TerminalView* terminal);
    ~FindBar() override;

    // The colour scheme's own colours.
    void setColors(cutefish::ui::Color background, cutefish::ui::Color text, bool darkMode,
                   cutefish::ui::Color error, cutefish::ui::Color accent);

    void open(const std::string& text);
    void close();

    std::function<void()> onClosed;

    cutefish::ui::SizeF sizeHint() const override { return {320, 40}; }
    bool acceptsPointer() const override { return true; }

protected:
    void layout() override;
    void paint(cutefish::ui::Painter& painter) override;

private:
    void search(bool forwards, bool fromSelection);
    void updateColors();

    Konsole::TerminalView* m_terminal;
    FindField* m_field;
    cutefish::ui::HeaderButton* m_previous;
    cutefish::ui::HeaderButton* m_next;
    cutefish::ui::HeaderButton* m_close;
    cutefish::base::Subscription m_findResult;
    bool m_notFound = false;
    cutefish::ui::Color m_background = cutefish::ui::Color::rgb(0x1A1B26);
    cutefish::ui::Color m_text = cutefish::ui::Color::rgb(0xC0CAF5);
    cutefish::ui::Color m_error = cutefish::ui::Color::rgb(0xF7768E);
    cutefish::ui::Color m_accent = cutefish::ui::Color::rgb(0x7AA2F7);
    bool m_darkMode = true;
};
