#ifndef TERMINALSCROLLER_H
#define TERMINALSCROLLER_H

#include <QAbstractAnimation>
#include <QElapsedTimer>
#include <QPointF>
#include <QPointer>

class QQuickWindow;
class QWheelEvent;

namespace Konsole
{

class TerminalDisplay;

// FishUI's WheelScroller on the one vertical axis of the scrollback, in
// pixels: a touchpad drags the text, a release coasts with friction, and the
// edges stretch and spring back.
class TerminalScroller : public QAbstractAnimation
{
public:
    explicit TerminalScroller(TerminalDisplay *display);

    bool handleWheel(QWheelEvent *event);
    void cancel(bool restoreBounds = true);
    bool isActive() const { return m_gesture || state() == Running; }
    int duration() const override;

protected:
    void updateCurrentTime(int time) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct Axis {
        qreal minimum;
        qreal maximum;
        qreal viewport;
        bool enabled;
    };

    Axis axis() const;
    qreal position() const;
    void setPosition(qreal position);
    void watchWindow(QQuickWindow *window);
    void begin(qint64 timestamp);
    void release(qint64 timestamp);
    void animate();
    void sampleVelocity(qreal delta, qint64 timestamp);
    qreal dragPosition(qreal position, const Axis &axis) const;
    qreal unboundedPosition(qreal position, const Axis &axis) const;
    void advance(qreal &position, qreal &velocity, const Axis &axis, qreal seconds);

    TerminalDisplay *m_display;
    QPointer<QQuickWindow> m_window;
    QElapsedTimer m_clock;
    qreal m_dragPosition = 0;
    qreal m_velocity = 0;
    qreal m_pendingDelta = 0;
    QPointF m_accumulatedDelta;
    qint64 m_lastSampleTime = 0;
    qint64 m_lastMovementTime = 0;
    int m_animationTime = 0;
    // A mostly sideways swipe leaves the scrollback alone.
    bool m_vertical = true;
    bool m_axisChosen = false;
    bool m_gesture = false;
    bool m_consumed = false;
    bool m_nativeMomentum = false;
    bool m_nativeBlocked = false;
    bool m_cancelled = false;
};

}

#endif // TERMINALSCROLLER_H
