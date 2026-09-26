#include <QQuickWindow>
#include <QTest>
#include <QWheelEvent>

#include "TerminalDisplay.h"
#include "ksession.h"

using namespace Konsole;

class TerminalScrollerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qputenv("KB_LAYOUT_DIR", KB_LAYOUTS_DIR);
    }

    void init()
    {
        m_window = new QQuickWindow;
        m_window->resize(400, 300);
        m_display = new TerminalDisplay(m_window->contentItem());
        m_session = new KSession;
        m_session->setShellProgram(QStringLiteral("/bin/sh"));
        m_session->setArgs({ QStringLiteral("-c"), QStringLiteral("seq 1 500; sleep 30") });
        // In QML's order: the session hears the size the display takes.
        m_display->setProperty("session", QVariant::fromValue(m_session));
        m_display->setWidth(400);
        m_display->setHeight(300);
        m_window->show();
        m_session->startShellProgram();
        QTRY_VERIFY(maximum() > 400);
        QTRY_COMPARE(value(), maximum());
        m_time = 1000;
    }

    void cleanup()
    {
        delete m_display;
        delete m_session;
        delete m_window;
    }

    // Whole lines scroll the view, the rest shifts the text; the line count follows the finger.
    void dragFollowsTheFinger()
    {
        const int lineHeight = m_display->property("fontMetrics").toSize().height();
        const int end = maximum();

        scroll(Qt::ScrollBegin, 0);
        scroll(Qt::ScrollUpdate, lineHeight * 3 + lineHeight / 2);
        QCOMPARE(value(), end - 4);
        scroll(Qt::ScrollUpdate, -lineHeight);
        QCOMPARE(value(), end - 3);

        // A slow release stays where the finger left it.
        m_time += 200;
        scroll(Qt::ScrollEnd, 0);
        QTest::qWait(100);
        QCOMPARE(value(), end - 3);
    }

    void releaseCoasts()
    {
        scroll(Qt::ScrollBegin, 0);
        for (int i = 0; i < 10; ++i) {
            m_time += 8;
            scroll(Qt::ScrollUpdate, 20);
        }
        const int released = value();
        scroll(Qt::ScrollEnd, 0);

        QTRY_VERIFY(value() < released - 10);
        // Friction brings it to rest.
        int previous = -1;
        QTRY_VERIFY_WITH_TIMEOUT([&] {
            const int current = value();
            const bool resting = current == previous;
            previous = current;
            return resting;
        }(), 5000);
    }

    // Past the top the text stretches and springs back to the first line.
    void topEdgeSpringsBack()
    {
        const int lineHeight = m_display->property("fontMetrics").toSize().height();
        scroll(Qt::ScrollBegin, 0);
        scroll(Qt::ScrollUpdate, (maximum() + 20) * lineHeight);
        QCOMPARE(value(), 0);
        m_time += 200;
        scroll(Qt::ScrollEnd, 0);
        QTest::qWait(1000);
        QCOMPARE(value(), 0);

        // Coming back down starts from the first line, not from the stretch.
        scroll(Qt::ScrollBegin, 0);
        scroll(Qt::ScrollUpdate, -lineHeight);
        QCOMPARE(value(), 1);
    }

    void keyPressStopsTheCoast()
    {
        scroll(Qt::ScrollBegin, 0);
        for (int i = 0; i < 10; ++i) {
            m_time += 8;
            scroll(Qt::ScrollUpdate, 30);
        }
        scroll(Qt::ScrollEnd, 0);
        QTest::qWait(50);

        QKeyEvent press(QEvent::KeyPress, Qt::Key_Shift, Qt::ShiftModifier);
        QCoreApplication::sendEvent(m_window, &press);
        const int stopped = value();
        QTest::qWait(200);
        QCOMPARE(value(), stopped);
    }

private:
    int value() const { return m_display->property("scrollbarCurrentValue").toInt(); }
    int maximum() const { return m_display->property("scrollbarMaximum").toInt(); }

    void scroll(Qt::ScrollPhase phase, int pixels)
    {
        const QPointF position(200, 150);
        QWheelEvent event(position, m_window->mapToGlobal(position), QPoint(0, pixels), QPoint(0, pixels),
                          Qt::NoButton, Qt::NoModifier, phase, false);
        event.setTimestamp(ulong(m_time));
        QCoreApplication::sendEvent(m_window, &event);
    }

    QQuickWindow *m_window = nullptr;
    TerminalDisplay *m_display = nullptr;
    KSession *m_session = nullptr;
    qint64 m_time = 0;
};

QTEST_MAIN(TerminalScrollerTest)

#include "terminalscroller_test.moc"
