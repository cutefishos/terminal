#include <utility>

#include <QKeyEvent>
#include <QTest>

#include "KeyboardTranslator.h"
#include "ScreenWindow.h"
#include "Vt102Emulation.h"

using namespace Konsole;

namespace {

// Stands in for the pty: the program's output goes in, and what the terminal
// answers or types comes out.
class Terminal
{
public:
    Terminal()
        : m_window(m_emulation.createWindow())
    {
        m_emulation.setKeyBindings(QString());
        QObject::connect(&m_emulation, &Emulation::sendData, [this](const char *data, int length) {
            m_sent.append(data, length);
        });
        QObject::connect(&m_emulation, &Emulation::handleCommandFromKeyboard,
                         [this](KeyboardTranslator::Command command) {
                             m_commands.append(command);
                         });
    }

    QByteArray print(const QByteArray &output)
    {
        m_emulation.receiveData(output.constData(), output.size());
        return std::exchange(m_sent, {});
    }

    QByteArray press(int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier,
                     const QString &text = QString())
    {
        QKeyEvent event(QEvent::KeyPress, key, modifiers, text);
        m_emulation.sendKeyEvent(&event, false);
        return std::exchange(m_sent, {});
    }

    // A cell of the screen's first line.
    Character cell(int column)
    {
        const Screen *screen = m_window->screen();
        QVector<Character> line(screen->getColumns());
        screen->getImage(line.data(), line.size(), screen->getHistLines(), screen->getHistLines());
        return line.at(column);
    }

    QString line(int length)
    {
        QString text;
        for (int column = 0; column < length; ++column)
            text += QChar(char16_t(cell(column).character));
        return text;
    }

    QList<KeyboardTranslator::Command> takeCommands() { return std::exchange(m_commands, {}); }

private:
    Vt102Emulation m_emulation;
    ScreenWindow *m_window;
    QByteArray m_sent;
    QList<KeyboardTranslator::Command> m_commands;
};

} // namespace

class Vt102EmulationTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qputenv("KB_LAYOUT_DIR", KB_LAYOUTS_DIR);
    }

    void keysStayAsTheyWereUntilAProgramAsks()
    {
        Terminal terminal;
        QCOMPARE(terminal.print("\033[?u"), QByteArray("\033[?0u"));
        QCOMPARE(terminal.press(Qt::Key_Return, Qt::ShiftModifier, QStringLiteral("\r")),
                 QByteArray("\033OM"));
        QCOMPARE(terminal.press(Qt::Key_A, Qt::ControlModifier, QStringLiteral("\x01")),
                 QByteArray("\x01"));
    }

    void flagsArePushedPoppedAndSet()
    {
        Terminal terminal;
        // Only disambiguation is supported, so the query reports it alone.
        terminal.print("\033[>5u");
        QCOMPARE(terminal.print("\033[?u"), QByteArray("\033[?1u"));
        terminal.print("\033[>0u");
        QCOMPARE(terminal.print("\033[?u"), QByteArray("\033[?0u"));
        terminal.print("\033[<u");
        QCOMPARE(terminal.print("\033[?u"), QByteArray("\033[?1u"));
        terminal.print("\033[<5u");
        QCOMPARE(terminal.print("\033[?u"), QByteArray("\033[?0u"));

        terminal.print("\033[=1u");
        QCOMPARE(terminal.print("\033[?u"), QByteArray("\033[?1u"));
        terminal.print("\033[=1;3u");
        QCOMPARE(terminal.print("\033[?u"), QByteArray("\033[?0u"));
        terminal.print("\033[=1;2u");
        QCOMPARE(terminal.print("\033[?u"), QByteArray("\033[?1u"));

        terminal.print("\033c");
        QCOMPARE(terminal.print("\033[?u"), QByteArray("\033[?0u"));
    }

    void eachScreenKeepsItsOwnFlags()
    {
        Terminal terminal;
        terminal.print("\033[>1u\033[?1049h");
        QCOMPARE(terminal.print("\033[?u"), QByteArray("\033[?0u"));
        terminal.print("\033[?1049l");
        QCOMPARE(terminal.print("\033[?u"), QByteArray("\033[?1u"));
    }

    void keysAreDisambiguated_data()
    {
        QTest::addColumn<int>("key");
        QTest::addColumn<Qt::KeyboardModifiers>("modifiers");
        QTest::addColumn<QString>("text");
        QTest::addColumn<QByteArray>("sent");

        const Qt::KeyboardModifiers none;
        const Qt::KeyboardModifiers shift = Qt::ShiftModifier;
        const Qt::KeyboardModifiers ctrl = Qt::ControlModifier;
        const Qt::KeyboardModifiers alt = Qt::AltModifier;
        const Qt::KeyboardModifiers keypad = Qt::KeypadModifier;

        QTest::newRow("enter") << int(Qt::Key_Return) << none << "\r" << QByteArray("\r");
        QTest::newRow("shift+enter") << int(Qt::Key_Return) << shift << "\r" << QByteArray("\033[13;2u");
        QTest::newRow("ctrl+enter") << int(Qt::Key_Return) << ctrl << "\r" << QByteArray("\033[13;5u");
        QTest::newRow("tab") << int(Qt::Key_Tab) << none << "\t" << QByteArray("\t");
        QTest::newRow("shift+tab") << int(Qt::Key_Backtab) << shift << "" << QByteArray("\033[9;2u");
        QTest::newRow("backspace") << int(Qt::Key_Backspace) << none << "\b" << QByteArray("\x7f");
        QTest::newRow("alt+backspace") << int(Qt::Key_Backspace) << alt << "\b" << QByteArray("\033[127;3u");
        QTest::newRow("escape") << int(Qt::Key_Escape) << none << "\x1b" << QByteArray("\033[27u");
        QTest::newRow("a") << int(Qt::Key_A) << none << "a" << QByteArray("a");
        QTest::newRow("shift+a") << int(Qt::Key_A) << shift << "A" << QByteArray("A");
        QTest::newRow("ctrl+a") << int(Qt::Key_A) << ctrl << "\x01" << QByteArray("\033[97;5u");
        QTest::newRow("ctrl+shift+c") << int(Qt::Key_C) << (ctrl | shift) << "\x03" << QByteArray("\033[99;6u");
        QTest::newRow("alt+b") << int(Qt::Key_B) << alt << "b" << QByteArray("\033[98;3u");
        QTest::newRow("ctrl+shift+1") << int(Qt::Key_Exclam) << (ctrl | shift) << "!" << QByteArray("\033[49;6u");
        QTest::newRow("ctrl+space") << int(Qt::Key_Space) << ctrl << " " << QByteArray("\033[32;5u");
        QTest::newRow("ctrl+cyrillic") << 0x0424 << ctrl << "ф" << QByteArray("\033[1092;5u");
        QTest::newRow("altgr text") << int(Qt::Key_At) << Qt::KeyboardModifiers(Qt::GroupSwitchModifier) << "@"
                                    << QByteArray("@");
        QTest::newRow("up") << int(Qt::Key_Up) << none << "" << QByteArray("\033[A");
        QTest::newRow("ctrl+up") << int(Qt::Key_Up) << ctrl << "" << QByteArray("\033[1;5A");
        QTest::newRow("f1") << int(Qt::Key_F1) << none << "" << QByteArray("\033[P");
        QTest::newRow("shift+f1") << int(Qt::Key_F1) << shift << "" << QByteArray("\033[1;2P");
        QTest::newRow("f3") << int(Qt::Key_F3) << none << "" << QByteArray("\033[13~");
        QTest::newRow("f13") << int(Qt::Key_F13) << none << "" << QByteArray("\033[57376u");
        QTest::newRow("ctrl+page down") << int(Qt::Key_PageDown) << ctrl << "" << QByteArray("\033[6;5~");
        QTest::newRow("delete") << int(Qt::Key_Delete) << none << "\x7f" << QByteArray("\033[3~");
        QTest::newRow("keypad enter") << int(Qt::Key_Enter) << keypad << "\r" << QByteArray("\033[57414u");
        QTest::newRow("keypad 1") << int(Qt::Key_1) << keypad << "1" << QByteArray("1");
        QTest::newRow("ctrl+keypad 1") << int(Qt::Key_1) << (keypad | ctrl) << "1" << QByteArray("\033[57400;5u");
    }

    void keysAreDisambiguated()
    {
        QFETCH(int, key);
        QFETCH(Qt::KeyboardModifiers, modifiers);
        QFETCH(QString, text);
        QFETCH(QByteArray, sent);

        Terminal terminal;
        terminal.print("\033[>1u");
        QCOMPARE(terminal.press(key, modifiers, text), sent);
    }

    void pastedTextGoesOutAsItIs()
    {
        Terminal terminal;
        terminal.print("\033[>1u");
        QCOMPARE(terminal.press(0, Qt::NoModifier, QStringLiteral("ls -l\r")), QByteArray("ls -l\r"));
    }

    void scrollingKeysStillScroll()
    {
        Terminal terminal;
        terminal.print("\033[>1u");
        QCOMPARE(terminal.press(Qt::Key_PageUp, Qt::ShiftModifier), QByteArray());
        const QList<KeyboardTranslator::Command> commands = terminal.takeCommands();
        QCOMPARE(commands.size(), 1);
        QVERIFY(commands.first() == KeyboardTranslator::ScrollPageUpCommand);
    }

    void prefixedSequencesPrintNothing()
    {
        Terminal terminal;
        terminal.print("a\033[=5;1ub\033[<uc\033[>4;2md");
        QCOMPARE(terminal.line(4), QStringLiteral("abcd"));
    }

    void subParametersStayWithTheirAttribute()
    {
        Terminal terminal;
        terminal.print("\033[4:3mA\033[4:0mB\033[58;5;1mC\033[38:2::255:0:0mD\033[m");
        QCOMPARE(terminal.line(4), QStringLiteral("ABCD"));

        QVERIFY(terminal.cell(0).rendition & RE_UNDERLINE);
        QVERIFY(!(terminal.cell(0).rendition & RE_ITALIC));
        QVERIFY(!(terminal.cell(1).rendition & RE_UNDERLINE));
        // An underline colour sets no other attribute.
        QCOMPARE(terminal.cell(2).rendition, terminal.cell(1).rendition);
        QVERIFY(terminal.cell(3).foregroundColor == CharacterColor(COLOR_SPACE_RGB, 0xff0000));
    }
};

QTEST_MAIN(Vt102EmulationTest)

#include "vt102emulation_test.moc"
