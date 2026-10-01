// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef UTILS_H
#define UTILS_H

#include <QObject>

class Utils : public QObject
{
    Q_OBJECT

public:
    static Utils *self();
    explicit Utils(QObject *parent = nullptr);

    Q_INVOKABLE QString text() const;
    Q_INVOKABLE void setText(const QString &text);
};

#endif // UTILS_H
