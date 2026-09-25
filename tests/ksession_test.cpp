#include <QDir>
#include <QTemporaryDir>
#include <QTest>

#include "ksession.h"

class KSessionTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qputenv("KB_LAYOUT_DIR", KB_LAYOUTS_DIR);
    }

    void titleFollowsTheProgramInFront()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString directoryName = QDir(directory.path()).dirName();

        KSession session;
        session.setShellProgram(QStringLiteral("/bin/sh"));
        session.setInitialWorkingDirectory(directory.path());

        // What a tab shows each time it hears the title changed.
        QStringList titles;
        connect(&session, &KSession::titleChanged, this, [&] {
            titles.append(session.getTitle());
        });

        session.startShellProgram();
        QTRY_COMPARE(session.getTitle(), directoryName);

        // A program that sets no title shows its name, and the prompt its directory.
        session.sendText(QStringLiteral("sleep 30\r"));
        QTRY_VERIFY(titles.contains(QStringLiteral("sleep")));
        session.sendText(QStringLiteral("\x03"));
        QTRY_COMPARE(titles.last(), directoryName);

        // A title the program sets wins over its name while it runs.
        session.sendText(QStringLiteral("sh -c 'printf \"\\033]2;hello\\007\"; sleep 30'\r"));
        QTRY_COMPARE(titles.last(), QStringLiteral("hello"));
        session.sendText(QStringLiteral("\x03"));
        QTRY_COMPARE(titles.last(), directoryName);

        // A title the shell sets at its prompt does not replace the directory.
        const qsizetype heard = titles.size();
        session.sendText(QStringLiteral("printf '\\033]0;user@host: ~\\007'\r"));
        QTRY_VERIFY(titles.size() > heard);
        QCOMPARE(titles.last(), directoryName);
    }
};

QTEST_MAIN(KSessionTest)

#include "ksession_test.moc"
