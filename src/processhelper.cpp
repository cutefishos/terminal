// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "processhelper.h"
#include "applicationlauncher.h"
#include <QDesktopServices>
#include <QDBusInterface>
#include <QApplication>
#include <QUrl>
#include <QDir>
#include <QFileInfo>

#include <QDBusMessage>
#include <QDBusConnection>
#include <QDBusPendingReply>

ProcessHelper *SELF = nullptr;

ProcessHelper *ProcessHelper::self()
{
    if (!SELF)
        SELF = new ProcessHelper;

    return SELF;
}

ProcessHelper::ProcessHelper(QObject *parent)
    : QObject(parent)
{

}

bool ProcessHelper::startDetached(const QString &program, const QStringList &arguments)
{
    return ApplicationLauncher::startDetached(QStringList{program} + arguments);
}

bool ProcessHelper::openUrl(const QString &url)
{
    QUrl _url = QUrl::fromUserInput(url);

    if (url.isNull())
        return false;

    return QDesktopServices::openUrl(_url);
}

bool ProcessHelper::openFileManager(const QString &url)
{
    QDBusInterface iface(QStringLiteral("org.freedesktop.FileManager1"),
                         QStringLiteral("/org/freedesktop/FileManager1"),
                         QStringLiteral("org.freedesktop.FileManager1"));

    if (iface.lastError().isValid())
        return false;

    iface.call("ShowFolders",
               QStringList() << QUrl::fromLocalFile(url).toString(),
               QString::number(QApplication::applicationPid()));

    return true;
}

bool ProcessHelper::showInFileManager(const QString &path)
{
    QDBusInterface iface(QStringLiteral("org.freedesktop.FileManager1"),
                         QStringLiteral("/org/freedesktop/FileManager1"),
                         QStringLiteral("org.freedesktop.FileManager1"));

    if (iface.lastError().isValid())
        return false;

    iface.call("ShowItems",
               QStringList() << QUrl::fromLocalFile(path).toString(),
               QString::number(QApplication::applicationPid()));

    return true;
}

QString ProcessHelper::existingPath(const QString &text, const QString &baseDir) const
{
    QString path = text.trimmed();

    if (path.isEmpty() || path.contains(QLatin1Char('\n')) || path.length() > 4096)
        return QString();

    // Shells print quoted names for paths with spaces.
    if (path.length() > 1
            && (path.startsWith(QLatin1Char('\'')) || path.startsWith(QLatin1Char('"')))
            && path.endsWith(path.at(0)))
        path = path.mid(1, path.length() - 2);

    if (path == QLatin1String("~") || path.startsWith(QLatin1String("~/")))
        path.replace(0, 1, QDir::homePath());

    const QFileInfo info(QDir(baseDir), path);
    return info.exists() ? info.absoluteFilePath() : QString();
}

bool ProcessHelper::newWindow(const QString &directory)
{
    return ApplicationLauncher::startDetached({ QCoreApplication::applicationFilePath(),
                                                QStringLiteral("--workdir"), directory },
                                              directory, QStringLiteral("cutefish-terminal"));
}
