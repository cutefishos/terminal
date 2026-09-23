/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     Reion Wong <reionwong@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
