// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fonts.h"

#include <QFontDatabase>

Fonts::Fonts(QObject *parent) : QObject(parent)
{
}

QStringList Fonts::families() const
{
    // Built on first use: checking every family for fixed pitch loads the
    // fonts, and only the settings dialog needs the list.
    if (!m_loaded) {
        m_loaded = true;

        const QStringList families = QFontDatabase::families();
        for (const QString &family : families) {
            if (QFontDatabase::isFixedPitch(family)) {
                m_families << family;
            }
        }
    }

    return m_families;
}
