// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PROCESSHELPER_H
#define PROCESSHELPER_H

#include <QObject>

class ProcessHelper : public QObject
{
    Q_OBJECT

public:
    static ProcessHelper *self();
    explicit ProcessHelper(QObject *parent = nullptr);

    Q_INVOKABLE bool startDetached(const QString &program, const QStringList &arguments);
    Q_INVOKABLE bool openUrl(const QString &url);
    Q_INVOKABLE bool openFileManager(const QString &url);
    Q_INVOKABLE bool showInFileManager(const QString &path);
    Q_INVOKABLE bool newWindow(const QString &directory);

    // The file or folder that selected text names, resolved against baseDir,
    // or an empty string when it does not name one.
    Q_INVOKABLE QString existingPath(const QString &text, const QString &baseDir) const;
};

#endif // PROCESSHELPER_H
