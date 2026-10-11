#include "AppController.h"

#include <QDateTime>
#include <QJsonArray>
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
    , m_captureInProgress(false)
    , m_captureReady(false)
    , m_captureName("Captured Signal")
    , m_capturedFrequency(38)
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

bool AppController::captureInProgress() const
{
    return m_captureInProgress;
}

bool AppController::captureReady() const
{
    return m_captureReady;
}

QString AppController::captureName() const
{
    return m_captureName;
}

void AppController::setApiUrl(const QString &url)
{
    if (m_apiUrl == url)
        return;

    m_apiUrl = url;
    emit apiUrlChanged();
    refreshStatus();
}

void AppController::setCaptureName(const QString &name)
{
    if (m_captureName == name)
        return;

    m_captureName = name;
    emit captureNameChanged();
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

    const QByteArray data = reply->readAll();
    const QString url = reply->url().toString();
    const bool wasConnected = m_serverConnected;

    if (reply->error() == QNetworkReply::NoError && !data.isEmpty()) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            const QJsonObject object = document.object();

            if (url.contains("/api/v1/capture")) {
                const QJsonObject signalObject = object.value("signal").toObject();
                if (object.value("success").toBool(false) && !signalObject.isEmpty()) {
                    m_capturedPulses.clear();
                    const QJsonArray pulseArray = signalObject.value("pulses").toArray();
                    for (const QJsonValue &pulse : pulseArray) {
                        m_capturedPulses.append(pulse.toInt());
                    }
                    m_capturedFrequency = signalObject.value("frequency").toInt(38);

                    m_captureReady = true;
                    m_captureInProgress = false;
                    m_captureName = m_captureName.isEmpty() ? "Captured Signal" : m_captureName;
                    emit captureReadyChanged();
                    emit captureInProgressChanged();
                    emit captureNameChanged();
                    m_status = "Signal captured";
                    emit statusChanged();
                } else {
                    m_captureReady = false;
                    m_captureInProgress = false;
                    m_status = "Capture failed";
                    emit captureReadyChanged();
                    emit captureInProgressChanged();
                    emit statusChanged();
                }
            } else if (url.contains("/api/v1/signals")) {
                m_captureReady = false;
                m_captureInProgress = false;
                m_capturedPulses.clear();
                m_status = "Signal saved";
                emit captureReadyChanged();
                emit captureInProgressChanged();
                emit statusChanged();
            } else {
                const bool connected = object.value("connected").toBool(false);
                const QString state = object.value("state").toString("Unknown");
                m_serverConnected = connected;
                m_status = connected ? QString("Connected (%1)").arg(state.isEmpty() ? "Ready" : state)
                                     : QString("Disconnected");
            }
        } else {
            if (!url.contains("/api/v1/capture") && !url.contains("/api/v1/signals")) {
                m_serverConnected = false;
                m_status = "Server responded, but status data is invalid";
            }
        }
    } else {
        if (!url.contains("/api/v1/capture") && !url.contains("/api/v1/signals")) {
            m_serverConnected = false;
            m_status = QString("Server offline: %1").arg(reply->errorString().isEmpty() ? "cannot reach API" : reply->errorString());
        }
    }

    reply->deleteLater();

    if (wasConnected != m_serverConnected) {
        emit serverConnectedChanged();
    }

    if (!url.contains("/api/v1/capture") && !url.contains("/api/v1/signals")) {
        emit statusChanged();
    }
}

void AppController::startCapture()
{
    if (m_captureInProgress || !m_serverConnected) {
        return;
    }

    m_captureReady = false;
    m_captureInProgress = true;
    m_capturedPulses.clear();
    emit captureReadyChanged();
    emit captureInProgressChanged();

    const QUrl requestUrl = QUrl(m_apiUrl).resolved(QUrl("/api/v1/capture"));
    QNetworkRequest request(requestUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body["timeout"] = 20.0;
    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    QNetworkReply *reply = m_networkManager->post(request, payload);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_captureInProgress = false;
            m_captureReady = false;
            m_status = "Capture failed";
            emit captureReadyChanged();
            emit captureInProgressChanged();
            emit statusChanged();
            return;
        }

        const QByteArray data = reply->readAll();
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            m_captureInProgress = false;
            m_captureReady = false;
            m_status = "Capture failed";
            emit captureReadyChanged();
            emit captureInProgressChanged();
            emit statusChanged();
            return;
        }

        const QJsonObject object = document.object();
        const QJsonObject signalObject = object.value("signal").toObject();
        if (object.value("success").toBool(false) && !signalObject.isEmpty()) {
            m_capturedPulses.clear();
            const QJsonArray pulseArray = signalObject.value("pulses").toArray();
            for (const QJsonValue &pulse : pulseArray) {
                m_capturedPulses.append(pulse.toInt());
            }
            m_capturedFrequency = signalObject.value("frequency").toInt(38);
            m_captureReady = true;
            m_captureInProgress = false;
            m_status = "Signal captured";
            emit captureReadyChanged();
            emit captureInProgressChanged();
            emit statusChanged();
        } else {
            m_captureInProgress = false;
            m_captureReady = false;
            m_status = "Capture timed out or failed";
            emit captureReadyChanged();
            emit captureInProgressChanged();
            emit statusChanged();
        }
    });
}

void AppController::saveCapturedSignal()
{
    if (m_capturedPulses.isEmpty()) {
        m_captureReady = false;
        emit captureReadyChanged();
        return;
    }

    if (m_captureName.trimmed().isEmpty()) {
        m_captureName = "Captured Signal";
        emit captureNameChanged();
    }

    QJsonObject payload;
    payload["name"] = m_captureName;
    payload["protocol"] = "Raw";
    payload["frequency"] = m_capturedFrequency;
    payload["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    payload["rawHex"] = "N/A";

    QJsonArray pulses;
    for (int pulse : m_capturedPulses) {
        pulses.append(pulse);
    }
    payload["pulses"] = pulses;

    const QUrl requestUrl = QUrl(m_apiUrl).resolved(QUrl("/api/v1/signals"));
    QNetworkRequest request(requestUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QNetworkReply *reply = m_networkManager->post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const QByteArray response = reply->readAll();
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);

        m_captureReady = false;
        m_captureInProgress = false;
        m_capturedPulses.clear();
        m_status = parseError.error == QJsonParseError::NoError ? "Signal saved" : "Save failed";
        emit captureReadyChanged();
        emit captureInProgressChanged();
        emit statusChanged();
    });
}

void AppController::cancelCapture()
{
    m_captureReady = false;
    m_captureInProgress = false;
    m_capturedPulses.clear();
    emit captureReadyChanged();
    emit captureInProgressChanged();
}

void AppController::transmitSignal()
{
    m_status = "Transmit requested";
    emit statusChanged();
}
