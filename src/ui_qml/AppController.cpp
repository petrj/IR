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
    , m_irDeviceConnected(false)
    , m_deviceState("Unknown")
    , m_apiHost()
    , m_serverRunning(false)
    , m_storage(ir::SignalStorage::getDefaultStoragePath())
{
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &AppController::onReplyFinished);
    connect(m_refreshTimer, &QTimer::timeout, this, &AppController::refreshStatus);
    m_refreshTimer->setInterval(1000);
    m_refreshTimer->start();
    refreshStatus();
    // initialize apiHost from apiUrl
    QUrl u(m_apiUrl);
    const QString hostPort = u.port() != -1 ? QString("%1:%2").arg(u.host()).arg(u.port()) : u.host();
    m_apiHost = hostPort;
    // fetch saved signals list periodically (or on demand)
    refreshSignals();
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

bool AppController::irDeviceConnected() const
{
    return m_irDeviceConnected;
}

QString AppController::deviceState() const
{
    return m_deviceState;
}

QString AppController::apiHost() const
{
    return m_apiHost;
}

bool AppController::serverRunning() const
{
    return m_serverRunning;
}

QVariantList AppController::savedSignals() const
{
    return m_signals;
}

void AppController::refreshSignals()
{
    m_signals.clear();
    for (size_t i = 0; i < m_storage.count(); ++i) {
        const auto& signal = m_storage.getSignals()[i];
        QVariantMap entry;
        entry["index"] = static_cast<int>(i);
        entry["name"] = QString::fromStdString(signal.name);
        entry["protocol"] = QString::fromStdString(signal.protocol);
        entry["frequency"] = static_cast<int>(signal.frequency_khz);
        entry["timestamp"] = QString::fromStdString(signal.timestamp);
        entry["pulseCount"] = static_cast<int>(signal.pulses.size());
        m_signals.append(entry);
    }
    emit savedSignalsChanged();
    if (m_signals.isEmpty()) {
        m_status = "No local saved signals";
        emit statusChanged();
    }
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
    const bool wasServerRunning = m_serverRunning;

    const bool replyOk = (reply->error() == QNetworkReply::NoError);

    if (replyOk && !data.isEmpty()) {
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
            } else {
                // status endpoint
                m_serverRunning = true;

                const bool connected = object.value("connected").toBool(false);
                const QString state = object.value("state").toString("Unknown");
                m_serverConnected = connected;
                m_status = connected ? QString("Connected (%1)").arg(state.isEmpty() ? "Ready" : state)
                                     : QString("Disconnected");

                // Extract device information if present. Support a few common response shapes.
                const bool wasDevice = m_irDeviceConnected;
                const QString prevDeviceState = m_deviceState;

                bool deviceConnected = false;
                QString deviceStateStr;
                if (object.contains("device") && object.value("device").isObject()) {
                    const QJsonObject dev = object.value("device").toObject();
                    deviceConnected = dev.value("connected").toBool(false);
                    deviceStateStr = dev.value("state").toString(state);
                } else {
                    deviceConnected = object.value("device_connected").toBool(false);
                    deviceStateStr = object.value("device_state").toString(state);
                    if (deviceStateStr.isEmpty())
                        deviceStateStr = state;
                }

                m_irDeviceConnected = deviceConnected;
                m_deviceState = deviceStateStr;

                if (wasDevice != m_irDeviceConnected) {
                    emit irDeviceConnectedChanged();
                }
                if (prevDeviceState != m_deviceState) {
                    emit deviceStateChanged();
                }
            }
        } else {
            if (!url.contains("/api/v1/capture")) {
                m_serverRunning = true;
                m_serverConnected = false;
                m_status = "Server responded, but status data is invalid";
            }
        }
    } else {
        if (!url.contains("/api/v1/capture")) {
            m_serverRunning = false;
            m_serverConnected = false;
            m_status = QString("Server offline: %1").arg(reply->errorString().isEmpty() ? "cannot reach API" : reply->errorString());
        }
    }

    reply->deleteLater();

    if (wasConnected != m_serverConnected) {
        emit serverConnectedChanged();
    }

    if (wasServerRunning != m_serverRunning) {
        emit serverRunningChanged();
    }

    if (!url.contains("/api/v1/capture")) {
        emit statusChanged();
    }

    // also ensure apiHost is up-to-date (apiUrl may have been changed externally)
    QUrl u(m_apiUrl);
    const QString hostPort = u.port() != -1 ? QString("%1:%2").arg(u.host()).arg(u.port()) : u.host();
    if (m_apiHost != hostPort) {
        m_apiHost = hostPort;
        emit apiHostChanged();
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

    ir::SavedIrSignal saved;
    saved.name = m_captureName.trimmed().toStdString();
    saved.protocol = "Raw";
    saved.frequency_khz = static_cast<uint8_t>(m_capturedFrequency);
    saved.timestamp = QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss").toStdString();
    saved.rawHex = "N/A";
    saved.pulses.assign(m_capturedPulses.begin(), m_capturedPulses.end());

    m_storage.addSignal(saved);
    refreshSignals();

    m_captureReady = false;
    m_captureInProgress = false;
    m_capturedPulses.clear();
    m_status = "Signal saved locally";
    emit captureReadyChanged();
    emit captureInProgressChanged();
    emit statusChanged();
}

void AppController::cancelCapture()
{
    m_captureReady = false;
    m_captureInProgress = false;
    m_capturedPulses.clear();
    emit captureReadyChanged();
    emit captureInProgressChanged();
}

void AppController::addSignalManually(const QString &name, const QString &protocol, int frequency, const QString &rawHex, const QVariantList &pulses)
{
    if (name.trimmed().isEmpty()) {
        m_status = "Manual signal name is required";
        emit statusChanged();
        return;
    }

    if (pulses.isEmpty() && rawHex.trimmed().isEmpty()) {
        m_status = "Manual signal needs pulse values or rawHex";
        emit statusChanged();
        return;
    }

    ir::SavedIrSignal signal;
    signal.name = name.trimmed().toStdString();
    signal.protocol = protocol.isEmpty() ? "Raw" : protocol.toStdString();
    signal.frequency_khz = static_cast<uint8_t>(frequency <= 0 ? 38 : frequency);
    signal.timestamp = QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss").toStdString();
    signal.rawHex = rawHex.trimmed().isEmpty() ? "N/A" : rawHex.trimmed().toStdString();

    if (!pulses.isEmpty()) {
        signal.pulses.clear();
        for (const QVariant &v : pulses) {
            bool ok = false;
            const int val = v.toInt(&ok);
            if (ok) {
                signal.pulses.push_back(val);
            }
        }
    }

    if (signal.name.empty()) {
        m_status = "Manual signal name is required";
        emit statusChanged();
        return;
    }

    if (signal.pulses.empty() && signal.rawHex == "N/A") {
        m_status = "Manual signal needs pulse values or rawHex";
        emit statusChanged();
        return;
    }

    m_storage.addSignal(signal);
    refreshSignals();
    m_status = "Signal saved locally";
    emit statusChanged();
}

void AppController::transmitSignal()
{
    m_status = "Transmit requested";
    emit statusChanged();
}
