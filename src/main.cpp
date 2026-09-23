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

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QLocale>
#include <QTranslator>
#include <QFile>
#include <QIcon>
#include <QCommandLineParser>
#include <QDir>

#include "cutefishapplication.h"
#include "processhelper.h"
#include "utils.h"
#include "fonts.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Cutefish::Application::prepare();
    app.setWindowIcon(QIcon::fromTheme("terminal"));

    // Everything after -e is the command and its own options, so it is split
    // off before the parser sees it.
    QStringList arguments = app.arguments();
    QStringList command;
    const int executeIndex = qMax(arguments.indexOf(QStringLiteral("-e")),
                                  arguments.indexOf(QStringLiteral("--execute")));
    if (executeIndex > 0) {
        command = arguments.mid(executeIndex + 1);
        arguments = arguments.mid(0, executeIndex);
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Cutefish Terminal"));
    parser.addHelpOption();
    parser.addOption(QCommandLineOption({ QStringLiteral("w"), QStringLiteral("workdir") },
                                        QStringLiteral("Start in <directory>."), QStringLiteral("directory")));
    parser.addOption(QCommandLineOption({ QStringLiteral("e"), QStringLiteral("execute") },
                                        QStringLiteral("Run <command> and its arguments instead of the shell.")));
    parser.process(arguments);

    // A single argument with spaces is a command line, as xterm reads it.
    if (command.size() == 1 && command.first().contains(QLatin1Char(' '))) {
        const QString shell = qEnvironmentVariable("SHELL", QStringLiteral("/bin/sh"));
        command = { shell, QStringLiteral("-c"), command.first() };
    }

    const QString workdir = parser.isSet(QStringLiteral("workdir"))
            ? QDir(parser.value(QStringLiteral("workdir"))).absolutePath()
            : QString();

    QQmlApplicationEngine engine;

    // Translations
    QLocale locale;
    QString qmFilePath = QString("%1/%2.qm").arg("/usr/share/cutefish-terminal/translations/").arg(locale.name());
    if (QFile::exists(qmFilePath)) {
        QTranslator *translator = new QTranslator(QGuiApplication::instance());
        if (translator->load(qmFilePath)) {
            QGuiApplication::installTranslator(translator);
        } else {
            translator->deleteLater();
        }
    }

    engine.rootContext()->setContextProperty("Process", new ProcessHelper);
    engine.rootContext()->setContextProperty("Utils", new Utils);
    engine.rootContext()->setContextProperty("Fonts", new Fonts);
    engine.rootContext()->setContextProperty("StartupDirectory", workdir);
    engine.rootContext()->setContextProperty("StartupCommand", command);

    engine.addImportPath(QStringLiteral("qrc:/"));
    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));

    return app.exec();
}
