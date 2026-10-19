#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QFile>
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

    QUrl qmlUrl;
#if defined(Q_OS_ANDROID)
    if (QFile::exists(QStringLiteral(":/qt/qml/IR/main.qml"))) {
        qmlUrl = QUrl(QStringLiteral("qrc:/qt/qml/IR/main.qml"));
    } else {
        qmlUrl = QUrl(QStringLiteral("qrc:/IR/main.qml"));
    }
#else
    const QString localPath = QStringLiteral(IR_QML_SOURCE_DIR) + "/main.qml";
    if (QFile::exists(localPath)) {
        qmlUrl = QUrl::fromLocalFile(localPath);
    } else if (QFile::exists(QStringLiteral(":/qt/qml/IR/main.qml"))) {
        qmlUrl = QUrl(QStringLiteral("qrc:/qt/qml/IR/main.qml"));
    } else {
        qmlUrl = QUrl(QStringLiteral("qrc:/IR/main.qml"));
    }
#endif

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
