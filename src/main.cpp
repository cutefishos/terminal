// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QDebug>
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
#include "windows.h"

int cutefish_application_main(int argc, char *argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QGuiApplication &app = *qGuiApp;
    app.setWindowIcon(QIcon::fromTheme("terminal"));

    // Everything after -e is the command and its own options, so it is split
    // off before the parser sees it.
    QStringList arguments = app.arguments();
    QStringList command;
    // The first of the two: a later one belongs to the command.
    const int shortIndex = arguments.indexOf(QStringLiteral("-e"));
    const int longIndex = arguments.indexOf(QStringLiteral("--execute"));
    const int executeIndex = shortIndex < 0 ? longIndex
            : longIndex < 0                 ? shortIndex
                                            : qMin(shortIndex, longIndex);
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

    engine.rootContext()->setContextProperty("Process", new ProcessHelper(&engine));
    engine.rootContext()->setContextProperty("Utils", new Utils(&engine));
    engine.rootContext()->setContextProperty("Fonts", new Fonts(&engine));
    engine.rootContext()->setContextProperty("StartupDirectory", workdir);
    engine.rootContext()->setContextProperty("StartupCommand", command);

    auto *windows = new Windows(&engine);
    engine.rootContext()->setContextProperty("Windows", windows);

    // One settings object for every window, so a change shows in all of them at once.
    QQmlComponent settingsComponent(&engine, QUrl(QStringLiteral("qrc:/qml/GlobalSettings.qml")));
    QObject *settings = settingsComponent.create();
    if (!settings) {
        qWarning() << settingsComponent.errorString();
        return 1;
    }
    settings->setParent(&engine);
    engine.rootContext()->setContextProperty("TerminalSettings", settings);

    engine.addImportPath(QStringLiteral("qrc:/"));
    if (!windows->create())
        return 1;

    return app.exec();
}
