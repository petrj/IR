#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QFile>
#include <QIcon>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>
#include <QUrl>

#include "AppController.h"

int main(int argc, char *argv[])
{
    qputenv("QT_QUICK_CONTROLS_STYLE", "Material");
    qputenv("QT_QUICK_CONTROLS_MATERIAL_THEME", "Dark");

    QGuiApplication app(argc, argv);
    app.setApplicationName("IR Transceiver");
    app.setOrganizationName("IR");

    QQuickStyle::setStyle("Material");

    QIcon appIcon;
    if (QFile::exists(QStringLiteral(":/qt/qml/IR/assets/icon.png"))) {
        appIcon = QIcon(QStringLiteral(":/qt/qml/IR/assets/icon.png"));
    } else if (QFile::exists(QStringLiteral(IR_QML_SOURCE_DIR) + "/assets/icon.png")) {
        appIcon = QIcon(QStringLiteral(IR_QML_SOURCE_DIR) + "/assets/icon.png");
    }
    if (!appIcon.isNull()) {
        app.setWindowIcon(appIcon);
    }

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
            } else if (auto *window = qobject_cast<QQuickWindow*>(obj)) {
#if defined(Q_OS_ANDROID)
                window->showMaximized();
                auto syncGeometry = [window]() {
                    if (auto *screen = window->screen()) {
                        const QRect avail = screen->availableGeometry();
                        if (window->geometry() != avail) {
                            window->setGeometry(avail);
                        }
                        window->requestUpdate();
                    }
                };
                if (auto *screen = window->screen()) {
                    QObject::connect(screen, &QScreen::geometryChanged, window, syncGeometry);
                    QObject::connect(screen, &QScreen::orientationChanged, window, syncGeometry);
                }
                QObject::connect(qApp, &QGuiApplication::primaryScreenChanged, window, [window, syncGeometry](QScreen *newScreen) {
                    if (newScreen) {
                        QObject::connect(newScreen, &QScreen::geometryChanged, window, syncGeometry);
                        QObject::connect(newScreen, &QScreen::orientationChanged, window, syncGeometry);
                        syncGeometry();
                    }
                });
                syncGeometry();
#endif
            }
        },
        Qt::QueuedConnection);

    engine.load(qmlUrl);
    return app.exec();
}
