#ifndef WINDOWS_H
#define WINDOWS_H

#include <QList>
#include <QObject>
#include <QPointer>
#include <QVariantMap>

class QQmlComponent;
class QQmlEngine;
class QQuickWindow;

// Owns the terminal windows of this process. A tab dragged out of a window
// keeps its shell, so the window it lands in has to live in the same process.
class Windows : public QObject
{
    Q_OBJECT

public:
    explicit Windows(QQmlEngine *engine);

    // A new window, with `properties` as its initial properties.
    Q_INVOKABLE QQuickWindow *create(const QVariantMap &properties = {});

    // Destroys `window` once the event loop it was released in is back.
    Q_INVOKABLE void release(QQuickWindow *window);

private:
    QQmlEngine *m_engine;
    QQmlComponent *m_component = nullptr;
    QList<QPointer<QQuickWindow>> m_windows;
};

#endif // WINDOWS_H
