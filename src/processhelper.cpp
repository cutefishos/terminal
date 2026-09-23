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
