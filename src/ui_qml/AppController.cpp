#include "AppController.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_status("Checking server...")
    , m_apiUrl("http://127.0.0.1:8080")
    , m_serverConnected(false)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_refreshTimer(new QTimer(this))
{
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &AppController::onReplyFinished);
    connect(m_refreshTimer, &QTimer::timeout, this, &AppController::refreshStatus);
    m_refreshTimer->setInterval(1000);
    m_refreshTimer->start();
    refreshStatus();
}

QString AppController::status() const
{
    return m_status;
}

QString AppController::apiUrl() const
{
    return m_apiUrl;
}

bool AppController::serverConnected() const
{
    return m_serverConnected;
}

void AppController::setApiUrl(const QString &url)
{
    if (m_apiUrl == url)
        return;

    m_apiUrl = url;
    emit apiUrlChanged();
    refreshStatus();
}

void AppController::refreshStatus()
{
    const QUrl requestUrl = QUrl(m_apiUrl).resolved(QUrl("/api/v1/status"));
    QNetworkRequest request(requestUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    m_networkManager->get(request);
}

void AppController::onReplyFinished(QNetworkReply *reply)
{
    if (!reply) {
        return;
    }

    const bool wasConnected = m_serverConnected;
    const QByteArray data = reply->readAll();

    if (reply->error() == QNetworkReply::NoError && !data.isEmpty()) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            const QJsonObject object = document.object();
            const bool connected = object.value("connected").toBool(false);
            const QString state = object.value("state").toString("Unknown");

            m_serverConnected = connected;
            m_status = connected ? QString("Connected (%1)").arg(state.isEmpty() ? "Ready" : state)
                                  : QString("Server reachable, device not connected");
        } else {
            m_serverConnected = false;
            m_status = "Server responded, but status data is invalid";
        }
    } else {
        m_serverConnected = false;
        m_status = QString("Server offline: %1").arg(reply->errorString().isEmpty() ? "cannot reach API" : reply->errorString());
    }

    reply->deleteLater();

    if (wasConnected != m_serverConnected) {
        emit serverConnectedChanged();
    }
    emit statusChanged();
}

void AppController::startCapture()
{
    m_status = "Capturing IR signal...";
    emit statusChanged();
}

void AppController::transmitSignal()
{
    m_status = "Transmit requested";
    emit statusChanged();
}
