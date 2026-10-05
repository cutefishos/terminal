#include "TerminalScroller.h"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QWheelEvent>

#include <cmath>

#include "TerminalDisplay.h"

using namespace Konsole;

// The values of FishUI's WheelScroller, so the terminal scrolls like every list.
static constexpr qreal friction = 4.5;
static constexpr qreal springFrequency = 18.0;
static constexpr qreal rubberBandResistance = 0.55;
static constexpr qreal minimumFlickSpeed = 40.0;
static constexpr qreal maximumFlickSpeed = 6000.0;
static constexpr qreal stopSpeed = 5.0;
static constexpr qint64 velocityTimeout = 80;

static qreal bounded(qreal position, qreal minimum, qreal maximum)
{
    return qBound(minimum, position, maximum);
}

TerminalScroller::TerminalScroller(TerminalDisplay *display)
    : QAbstractAnimation(display)
    , m_display(display)
{
    m_clock.start();
    watchWindow(display->window());
    connect(display, &QQuickItem::windowChanged, this, &TerminalScroller::watchWindow);
    connect(display, &QQuickItem::visibleChanged, this, [this] {
        if (!m_display->isVisible())
            cancel();
    });
}

TerminalScroller::Axis TerminalScroller::axis() const
{
    const qreal maximum = m_display->scrollPixelMaximum();
    return {0, maximum, m_display->height(), maximum > 0};
}

qreal TerminalScroller::position() const
{
    return m_display->scrollPixelPosition();
}

void TerminalScroller::setPosition(qreal position)
{
    m_display->setScrollPixelPosition(position);
}

void TerminalScroller::watchWindow(QQuickWindow *window)
{
    cancel();
    if (m_window)
        m_window->removeEventFilter(this);
    m_window = window;
    if (m_window)
        m_window->installEventFilter(this);
}

bool TerminalScroller::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::TouchBegin:
    case QEvent::KeyPress:
    case QEvent::WindowDeactivate:
    case QEvent::Hide:
        cancel();
        break;
    case QEvent::Wheel:
        if (static_cast<QWheelEvent *>(event)->phase() == Qt::ScrollBegin && isActive()) {
            // A new contact stops the coast; an overscrolled view still settles.
            stop();
            m_velocity = 0;
            m_gesture = false;
            m_cancelled = true;
            animate();
        }
        break;
    default:
        break;
    }
    return QAbstractAnimation::eventFilter(watched, event);
}

void TerminalScroller::cancel(bool restoreBounds)
{
    const bool wasActive = isActive();
    stop();
    m_gesture = false;
    m_consumed = false;
    m_velocity = 0;
    m_cancelled |= wasActive;
    if (!restoreBounds || !wasActive)
        return;

    const Axis a = axis();
    setPosition(bounded(position(), a.minimum, a.maximum));
}

qreal TerminalScroller::dragPosition(qreal position, const Axis &axis) const
{
    const qreal edge = bounded(position, axis.minimum, axis.maximum);
    if (axis.viewport <= 0)
        return edge;

    const qreal distance = position - edge;
    return edge + distance * rubberBandResistance
        / (1.0 + std::abs(distance) * rubberBandResistance / axis.viewport);
}

qreal TerminalScroller::unboundedPosition(qreal position, const Axis &axis) const
{
    const qreal edge = bounded(position, axis.minimum, axis.maximum);
    const qreal distance = position - edge;
    if (axis.viewport <= 0)
        return edge;

    return edge + distance / (rubberBandResistance
        * qMax(0.01, 1.0 - std::abs(distance) / axis.viewport));
}

void TerminalScroller::begin(qint64 timestamp)
{
    stop();
    m_gesture = true;
    m_consumed = false;
    m_nativeMomentum = false;
    m_nativeBlocked = false;
    m_cancelled = false;
    m_velocity = 0;
    m_pendingDelta = 0;
    m_accumulatedDelta = {};
    m_vertical = true;
    m_axisChosen = false;
    m_lastSampleTime = timestamp;
    m_lastMovementTime = timestamp;
    m_dragPosition = unboundedPosition(position(), axis());
}

void TerminalScroller::sampleVelocity(qreal delta, qint64 timestamp)
{
    m_pendingDelta += delta;
    if (delta != 0)
        m_lastMovementTime = timestamp;
    const qint64 elapsed = timestamp - m_lastSampleTime;
    if (elapsed <= 0)
        return;

    const qreal sample = m_pendingDelta * (1000.0 / elapsed);
    const qreal weight = 1.0 - std::exp(-elapsed / 35.0);
    if (elapsed > velocityTimeout || m_velocity * sample <= 0)
        m_velocity = bounded(sample, -maximumFlickSpeed, maximumFlickSpeed);
    else
        m_velocity = bounded(m_velocity + weight * (sample - m_velocity), -maximumFlickSpeed, maximumFlickSpeed);
    m_pendingDelta = 0;
    m_lastSampleTime = timestamp;
}

// Only precise scrolling comes here; a wheel keeps stepping whole lines.
bool TerminalScroller::handleWheel(QWheelEvent *event)
{
    const Axis a = axis();
    const auto phase = event->phase();
    const qint64 timestamp = event->timestamp() ? qint64(event->timestamp()) : m_clock.elapsed();

    if (phase == Qt::ScrollBegin)
        begin(timestamp);
    else if (phase == Qt::ScrollUpdate && !m_gesture && !m_cancelled)
        begin(timestamp);

    if (phase == Qt::ScrollEnd) {
        if (!m_gesture)
            return false;
        // Some backends include the last movement in the end event.
        if (!event->pixelDelta().isNull()) {
            QWheelEvent last(event->position(), event->globalPosition(), event->pixelDelta(), {},
                             event->buttons(), event->modifiers(),
                             m_nativeMomentum ? Qt::ScrollMomentum : Qt::ScrollUpdate, event->inverted());
            last.setTimestamp(event->timestamp());
            handleWheel(&last);
        }
        const bool consumed = m_consumed;
        release(timestamp);
        return consumed;
    }

    if (phase == Qt::ScrollMomentum) {
        if (m_cancelled)
            return true;
        if (!m_nativeMomentum) {
            stop();
            m_velocity = 0;
            m_nativeMomentum = true;
            m_gesture = true;
        }
    } else if (phase == Qt::ScrollUpdate && m_cancelled) {
        return true;
    } else if (phase == Qt::NoScrollPhase) {
        cancel();
    }

    const QPointF pixels = event->pixelDelta();
    if (m_gesture && !m_nativeMomentum && !m_axisChosen) {
        m_accumulatedDelta += pixels;
        const qreal dx = std::abs(m_accumulatedDelta.x());
        const qreal dy = std::abs(m_accumulatedDelta.y());
        if (qMax(dx, dy) >= 6) {
            m_vertical = !(dx > 2.5 * dy);
            m_axisChosen = true;
        }
    }

    const qreal delta = (!a.enabled || (m_gesture && !m_vertical)) ? 0 : pixels.y();
    if (delta == 0)
        return m_gesture && m_consumed;

    const qreal previous = position();
    qreal next = previous;
    if (m_gesture && !m_nativeMomentum) {
        m_dragPosition -= delta;
        next = dragPosition(m_dragPosition, a);
        sampleVelocity(next - previous, timestamp);
    } else if (phase == Qt::ScrollMomentum) {
        if (!m_nativeBlocked)
            next = dragPosition(previous - delta, a);
        m_nativeBlocked |= next <= a.minimum || next >= a.maximum;
        if (next < a.minimum || next > a.maximum)
            animate();
    } else {
        next = bounded(previous - delta, a.minimum, a.maximum);
    }
    setPosition(next);
    m_consumed |= position() != previous;
    return position() != previous || (m_gesture && m_consumed);
}

void TerminalScroller::release(qint64 timestamp)
{
    m_gesture = false;
    if (m_nativeMomentum || timestamp - m_lastMovementTime > velocityTimeout)
        m_velocity = 0;
    if (std::abs(m_velocity) < minimumFlickSpeed)
        m_velocity = 0;
    animate();
}

void TerminalScroller::animate()
{
    if (state() == Running)
        return;
    m_animationTime = 0;
    start();
}

int TerminalScroller::duration() const
{
    return -1;
}

void TerminalScroller::advance(qreal &position, qreal &velocity, const Axis &axis, qreal seconds)
{
    const qreal edge = bounded(position, axis.minimum, axis.maximum);
    if (!axis.enabled) {
        position = edge;
        velocity = 0;
        return;
    }

    if (edge != position) {
        const qreal offset = position - edge;
        const qreal coefficient = velocity + springFrequency * offset;
        const qreal decay = std::exp(-springFrequency * seconds);
        position = edge + (offset + coefficient * seconds) * decay;
        velocity = (velocity - springFrequency * coefficient * seconds) * decay;
        if (std::abs(position - edge) < 0.5 && std::abs(velocity) < stopSpeed) {
            position = edge;
            velocity = 0;
        }
    } else {
        const qreal initialPosition = position;
        const qreal initialVelocity = velocity;
        const qreal decay = std::exp(-friction * seconds);
        position += velocity * (1.0 - decay) / friction;
        velocity *= decay;
        const qreal limit = bounded(position, axis.minimum, axis.maximum);
        if (limit != position) {
            // Split the frame at the edge so rebound does not depend on refresh rate.
            const qreal crossing = -std::log1p(-(limit - initialPosition) * friction / initialVelocity)
                / friction;
            const qreal remaining = qMax(0.0, seconds - crossing);
            const qreal edgeVelocity = initialVelocity * std::exp(-friction * crossing);
            const qreal springDecay = std::exp(-springFrequency * remaining);
            position = limit + edgeVelocity * remaining * springDecay;
            velocity = edgeVelocity * (1.0 - springFrequency * remaining) * springDecay;
        } else if (std::abs(velocity) < stopSpeed) {
            velocity = 0;
        }
    }
}

void TerminalScroller::updateCurrentTime(int time)
{
    const qreal seconds = (time - m_animationTime) / 1000.0;
    m_animationTime = time;
    if (seconds <= 0)
        return;

    const Axis a = axis();
    qreal next = position();
    advance(next, m_velocity, a, seconds);
    setPosition(next);
    if (m_velocity == 0 && next >= a.minimum && next <= a.maximum)
        stop();
}
