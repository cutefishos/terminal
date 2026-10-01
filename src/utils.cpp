// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils.h"
#include <QApplication>
#include <QClipboard>

static Utils *SELF = nullptr;

Utils *Utils::self()
{
    if (!SELF)
        SELF = new Utils;

    return SELF;
}

Utils::Utils(QObject *parent)
    : QObject(parent)
{

}

QString Utils::text() const
{
    return qApp->clipboard()->text();
}

void Utils::setText(const QString &text)
{
    qApp->clipboard()->setText(text);
}
