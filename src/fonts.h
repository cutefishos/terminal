// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FONTS_H
#define FONTS_H

#include <QObject>
#include <QStringList>

class Fonts : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList families READ families NOTIFY familiesChanged)

public:
    explicit Fonts(QObject *parent = nullptr);

    QStringList families() const;

Q_SIGNALS:
    void familiesChanged();

private:
    mutable QStringList m_families;
    mutable bool m_loaded = false;
};

#endif // FONTS_H
