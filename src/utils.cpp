// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils.h"
#include <QClipboard>
#include <QGuiApplication>

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
    return QGuiApplication::clipboard()->text();
}

void Utils::setText(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}
