#include "windows.h"

#include <QCoreApplication>
#include <QDebug>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>

Windows::Windows(QQmlEngine *engine)
    : QObject(engine)
    , m_engine(engine)
{
    // Before the engine goes: its windows run QML from it.
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] {
        for (const QPointer<QQuickWindow> &window : std::as_const(m_windows))
            delete window.data();
        m_windows.clear();
    });
}

QQuickWindow *Windows::create(const QVariantMap &properties)
{
    if (!m_component)
        m_component = new QQmlComponent(m_engine, QUrl(QStringLiteral("qrc:/qml/main.qml")), this);

    QObject *object = m_component->createWithInitialProperties(properties, m_engine->rootContext());
    auto *window = qobject_cast<QQuickWindow *>(object);
    if (!window) {
        qWarning().noquote() << "Cannot create a terminal window:" << m_component->errorString();
        delete object;
        return nullptr;
    }

    QQmlEngine::setObjectOwnership(window, QQmlEngine::CppOwnership);
    m_windows.removeAll(nullptr);
    m_windows.append(window);
    return window;
}

void Windows::release(QQuickWindow *window)
{
    if (window)
        window->deleteLater();
}
