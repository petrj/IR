#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>

#include "AppController.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("IR Transceiver");
    app.setOrganizationName("IR");

    AppController controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("appController", &controller);

    const QUrl qmlUrl = QUrl::fromLocalFile(QStringLiteral(IR_QML_SOURCE_DIR) + "/main.qml");
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [qmlUrl](QObject *obj, const QUrl &url) {
            if (!obj && url == qmlUrl) {
                qCritical() << "Failed to load QML file:" << qmlUrl.toString();
            }
        },
        Qt::QueuedConnection);

    engine.load(qmlUrl);
    return app.exec();
}
