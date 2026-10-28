#include "AppController.h"

#include <sstream>
#include <iomanip>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>
#include <QTime>
#include <QTimer>
#include <QUrl>

static std::string getAppStoragePath()
{
#if defined(Q_OS_ANDROID)
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    return (dataDir + "/saved_ir_signals.json").toStdString();
#else
    return ir::SignalStorage::getDefaultStoragePath();
#endif
}

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_status("Checking server...")
    , m_apiUrl("http://127.0.0.1:8080")
    , m_serverConnected(false)
    , m_captureInProgress(false)
    , m_captureReady(false)
    , m_captureName("Captured Signal")
    , m_captureRcdName("Default Remote Device")
    , m_selectedDeviceFilter("All Devices")
    , m_capturedFrequency(38)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_refreshTimer(new QTimer(this))
    , m_irDeviceConnected(false)
    , m_deviceState("Unknown")
    , m_usbStatusDetails("Loading USB status...")
    , m_serverStatusDetails("Loading server status...")
    , m_apiHost()
    , m_serverRunning(false)
    , m_storage(getAppStoragePath())
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
    updateServerStatusDetails();
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

QString AppController::captureRcdName() const
{
    return m_captureRcdName;
}

QStringList AppController::rcdList() const
{
    QStringList list;
    auto names = m_storage.getDeviceNames();
    for (const auto& n : names) {
        list.append(QString::fromStdString(n));
    }
    if (list.isEmpty()) {
        list.append("Default Remote Device");
    }
    return list;
}

QString AppController::selectedDeviceFilter() const
{
    return m_selectedDeviceFilter;
}

bool AppController::irDeviceConnected() const
{
    return m_irDeviceConnected;
}

QString AppController::deviceState() const
{
    return m_deviceState;
}

QString AppController::usbStatusDetails() const
{
    return m_usbStatusDetails;
}

QString AppController::serverStatusDetails() const
{
    return m_serverStatusDetails;
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
    const auto& allSignals = m_storage.getSignals();
    for (size_t i = 0; i < allSignals.size(); ++i) {
        const auto& signal = allSignals[i];
        if (m_selectedDeviceFilter != "All Devices" && !m_selectedDeviceFilter.isEmpty()) {
            if (QString::fromStdString(signal.rcdName) != m_selectedDeviceFilter) {
                continue;
            }
        }
        QVariantMap entry;
        entry["index"] = static_cast<int>(i);
        entry["name"] = QString::fromStdString(signal.name);
        entry["rcdName"] = QString::fromStdString(signal.rcdName);
        entry["protocol"] = QString::fromStdString(signal.protocol);
        entry["frequency"] = static_cast<int>(signal.frequency_khz);
        entry["timestamp"] = QString::fromStdString(signal.timestamp);
        entry["pulseCount"] = static_cast<int>(signal.pulses.size());
        entry["shellCommand"] = QString::fromStdString(signal.shellCommand);
        m_signals.append(entry);
    }
    emit savedSignalsChanged();
    emit rcdListChanged();
    emit armedSignalsChanged();
    m_armedTriggerCount = armedSignals().size();
    emit armedTriggerCountChanged();
    if (m_signals.isEmpty()) {
        m_status = "No saved signals";
        emit statusChanged();
    }
}

void AppController::setApiUrl(const QString &url)
{
    if (m_apiUrl == url)
        return;

    m_apiUrl = url;
    emit apiUrlChanged();
    QUrl u(m_apiUrl);
    const QString hostPort = u.port() != -1 ? QString("%1:%2").arg(u.host()).arg(u.port()) : u.host();
    if (m_apiHost != hostPort) {
        m_apiHost = hostPort;
        emit apiHostChanged();
    }
    updateServerStatusDetails();
    refreshStatus();
}

void AppController::setCaptureName(const QString &name)
{
    if (m_captureName == name)
        return;

    m_captureName = name;
    emit captureNameChanged();
}

void AppController::setCaptureRcdName(const QString &name)
{
    if (m_captureRcdName == name)
        return;

    m_captureRcdName = name;
    emit captureRcdNameChanged();
}

void AppController::setSelectedDeviceFilter(const QString &filter)
{
    if (m_selectedDeviceFilter == filter)
        return;

    m_selectedDeviceFilter = filter;
    emit selectedDeviceFilterChanged();
    refreshSignals();
}

void AppController::refreshStatus()
{
    const QUrl requestUrl = QUrl(m_apiUrl).resolved(QUrl("/api/v1/status"));
    QNetworkRequest request(requestUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (m_usbStatusDetails.isEmpty() || m_usbStatusDetails == "Loading USB status...") {
        m_usbStatusDetails = "Loading USB status...";
        emit usbStatusDetailsChanged();
    }
    m_networkManager->get(request);
}

void AppController::refreshUsbStatusDialog()
{
    refreshStatus();
    m_usbStatusDetails = "Loading USB status...";
    emit usbStatusDetailsChanged();
}

void AppController::refreshServerStatusDialog()
{
    refreshStatus();
    updateServerStatusDetails();
}

void AppController::onReplyFinished(QNetworkReply *reply)
{
    if (!reply) {
        return;
    }

    const QString url = reply->url().toString();
    // Only process status replies in this global handler.
    // Other endpoints (/api/v1/send, /api/v1/capture, etc.) attach their own reply finished handlers.
    if (!url.contains("/api/v1/status")) {
        return;
    }

    const QByteArray data = reply->readAll();
    const bool wasConnected = m_serverConnected;
    const bool wasServerRunning = m_serverRunning;

    const bool replyOk = (reply->error() == QNetworkReply::NoError);

    if (replyOk && !data.isEmpty()) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            const QJsonObject object = document.object();

            // status endpoint
            m_serverRunning = true;

            const bool connected = object.value("connected").toBool(false);
            const QString state = object.value("state").toString("Unknown");
            m_serverConnected = connected;
            m_status = connected ? QString("Connected (%1)").arg(state.isEmpty() ? "Ready" : state)
                                 : QString("Disconnected");

            // Extract device information if present. Support the API's actual response shape
            // as well as older fallback payloads.
            const bool wasDevice = m_irDeviceConnected;
            const QString prevDeviceState = m_deviceState;

            bool deviceConnected = object.value("connected").toBool(false);
            QString deviceStateStr = object.value("state").toString(state);

            if (object.contains("device") && object.value("device").isObject()) {
                const QJsonObject dev = object.value("device").toObject();
                if (dev.contains("connected")) {
                    deviceConnected = dev.value("connected").toBool(deviceConnected);
                }
                if (dev.contains("state")) {
                    deviceStateStr = dev.value("state").toString(deviceStateStr);
                }
            } else {
                deviceConnected = object.value("device_connected").toBool(deviceConnected);
                deviceStateStr = object.value("device_state").toString(deviceStateStr);
                if (deviceStateStr.isEmpty())
                    deviceStateStr = state;
            }

            m_irDeviceConnected = deviceConnected;
            m_deviceState = deviceStateStr;

            const QJsonObject deviceObject = object.value("device").toObject();
            const QString vendorId = deviceObject.value("vendorId").toString();
            const QString productId = deviceObject.value("productId").toString();
            const QString manufacturer = deviceObject.value("manufacturer").toString();
            const QString product = deviceObject.value("product").toString();
            const QString serialNumber = deviceObject.value("serialNumber").toString();
            const QString firmwareVersion = deviceObject.value("firmwareVersion").toString();
            const QString usbDetails = QString(
                "Connected: %1\nState: %2\nManufacturer: %3\nProduct: %4\nVendor ID: %5\nProduct ID: %6\nSerial: %7\nFirmware: %8"
            )
                .arg(deviceConnected ? "Yes" : "No")
                .arg(deviceStateStr.isEmpty() ? "Unknown" : deviceStateStr)
                .arg(manufacturer.isEmpty() ? "N/A" : manufacturer)
                .arg(product.isEmpty() ? "N/A" : product)
                .arg(vendorId.isEmpty() ? "N/A" : vendorId)
                .arg(productId.isEmpty() ? "N/A" : productId)
                .arg(serialNumber.isEmpty() ? "N/A" : serialNumber)
                .arg(firmwareVersion.isEmpty() ? "N/A" : firmwareVersion);
            if (m_usbStatusDetails != usbDetails) {
                m_usbStatusDetails = usbDetails;
                emit usbStatusDetailsChanged();
            }

            if (wasDevice != m_irDeviceConnected) {
                emit irDeviceConnectedChanged();
            }
            if (prevDeviceState != m_deviceState) {
                emit deviceStateChanged();
            }
        } else {
            m_serverRunning = true;
            m_serverConnected = false;
            m_status = "Server responded, but status data is invalid";
            m_usbStatusDetails = "USB status unavailable\nServer responded, but status data is invalid.";
            emit usbStatusDetailsChanged();
        }
    } else {
        m_serverRunning = false;
        m_serverConnected = false;
        m_status = QString("Server offline: %1").arg(reply->errorString().isEmpty() ? "cannot reach API" : reply->errorString());
        m_usbStatusDetails = QString("USB status unavailable\n%1").arg(m_status);
        emit usbStatusDetailsChanged();
    }

    reply->deleteLater();

    if (wasConnected != m_serverConnected) {
        emit serverConnectedChanged();
    }

    if (wasServerRunning != m_serverRunning) {
        emit serverRunningChanged();
    }

    emit statusChanged();

    // also ensure apiHost is up-to-date (apiUrl may have been changed externally)
    QUrl u(m_apiUrl);
    const QString hostPort = u.port() != -1 ? QString("%1:%2").arg(u.host()).arg(u.port()) : u.host();
    if (m_apiHost != hostPort) {
        m_apiHost = hostPort;
        emit apiHostChanged();
    }
    updateServerStatusDetails();
}

void AppController::updateServerStatusDetails()
{
    const QString host = m_apiHost.isEmpty() ? QStringLiteral("-") : m_apiHost;
    const QString statusStr = m_serverRunning ? QStringLiteral("Running") : QStringLiteral("Stopped");
    const QString detailsStr = m_status.isEmpty() ? statusStr : m_status;
    const QString serverDetails = QString(
        "Server Status: %1\nConfigured IP: %2\nAPI URL: %3\nDetails: %4"
    )
        .arg(statusStr)
        .arg(host)
        .arg(m_apiUrl.isEmpty() ? QStringLiteral("-") : m_apiUrl)
        .arg(detailsStr);

    if (m_serverStatusDetails != serverDetails) {
        m_serverStatusDetails = serverDetails;
        emit serverStatusDetailsChanged();
    }
}

QVariantMap AppController::getSignalDetails(int index) const
{
    QVariantMap details;
    if (index < 0 || static_cast<size_t>(index) >= m_storage.count()) {
        return details;
    }

    const auto signal = m_storage.getSignals()[index];
    details["index"] = index;
    details["name"] = QString::fromStdString(signal.name);
    details["rcdName"] = QString::fromStdString(signal.rcdName);
    details["protocol"] = QString::fromStdString(signal.protocol);
    details["frequency"] = static_cast<int>(signal.frequency_khz);
    details["timestamp"] = QString::fromStdString(signal.timestamp);
    details["rawHex"] = QString::fromStdString(signal.rawHex);
    details["shellCommand"] = QString::fromStdString(signal.shellCommand);

    QVariantList pulses;
    for (const int pulse : signal.pulses) {
        pulses.append(pulse);
    }
    details["pulses"] = pulses;
    return details;
}

void AppController::updateSignal(int index, const QString &name, const QString &protocol, int frequency, const QString &rawHex, const QVariantList &pulses, const QString &rcdName, const QString &shellCommand)
{
    if (index < 0 || static_cast<size_t>(index) >= m_storage.count()) {
        m_status = "Invalid saved signal index";
        emit statusChanged();
        return;
    }

    if (name.trimmed().isEmpty()) {
        m_status = "Signal name is required";
        emit statusChanged();
        return;
    }

    if (pulses.isEmpty() && rawHex.trimmed().isEmpty()) {
        m_status = "Signal needs pulse values or rawHex";
        emit statusChanged();
        return;
    }

    auto existingOpt = m_storage.getSignal(static_cast<size_t>(index));
    if (!existingOpt) {
        m_status = "Signal not found";
        emit statusChanged();
        return;
    }

    ir::SavedIrSignal updated;
    const auto existing = *existingOpt;
    updated.name = name.trimmed().toStdString();
    std::string targetRcd = rcdName.trimmed().isEmpty() ? existing.rcdName : rcdName.trimmed().toStdString();
    updated.rcdName = targetRcd;
    updated.protocol = protocol.isEmpty() ? "Raw" : protocol.toStdString();
    updated.frequency_khz = static_cast<uint8_t>(frequency <= 0 ? 38 : frequency);
    updated.timestamp = QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss").toStdString();
    updated.rawHex = rawHex.trimmed().isEmpty() ? "N/A" : rawHex.trimmed().toStdString();
    updated.shellCommand = shellCommand.trimmed().toStdString();
    updated.pulses.clear();
    for (const QVariant &v : pulses) {
        bool ok = false;
        const int value = v.toInt(&ok);
        if (ok) {
            updated.pulses.push_back(value);
        }
    }

    if (updated.pulses.empty() && updated.rawHex == "N/A") {
        m_status = "Signal needs pulse values or rawHex";
        emit statusChanged();
        return;
    }

    if (!m_storage.updateSignal(static_cast<size_t>(index), updated)) {
        m_status = "Failed to update signal";
        emit statusChanged();
        return;
    }

    refreshSignals();
    m_status = "Signal updated";
    emit statusChanged();
}

void AppController::startCapture()
{
    if (m_triggerModeActive) {
        stopTriggerMode();
    }

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

void AppController::saveCapturedSignal(const QString &rcdName, const QString &signalName, const QString &shellCommand)
{
    if (m_capturedPulses.isEmpty()) {
        m_captureReady = false;
        emit captureReadyChanged();
        return;
    }

    QString sigName = !signalName.trimmed().isEmpty() ? signalName.trimmed() : m_captureName.trimmed();
    if (sigName.isEmpty()) {
        sigName = "Captured Signal";
    }

    QString targetRcd = !rcdName.trimmed().isEmpty() ? rcdName.trimmed() : m_captureRcdName.trimmed();
    if (targetRcd.isEmpty()) {
        targetRcd = "Default Remote Device";
    }

    m_captureName = sigName;
    m_captureRcdName = targetRcd;
    emit captureNameChanged();
    emit captureRcdNameChanged();

    ir::SavedIrSignal saved;
    saved.name = sigName.toStdString();
    saved.rcdName = targetRcd.toStdString();
    saved.protocol = "Raw";
    saved.frequency_khz = static_cast<uint8_t>(m_capturedFrequency);
    saved.timestamp = QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss").toStdString();
    saved.rawHex = "N/A";
    saved.shellCommand = shellCommand.trimmed().toStdString();
    saved.pulses.assign(m_capturedPulses.begin(), m_capturedPulses.end());

    ir::DecodedCode necCode;
    if (ir::IrProtocol::decodeNec(saved.pulses, necCode)) {
        saved.protocol = "NEC";
        std::ostringstream ss;
        ss << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << necCode.rawValue;
        saved.rawHex = ss.str();
    }

    m_storage.addSignal(saved, targetRcd.toStdString());
    refreshSignals();

    m_captureReady = false;
    m_captureInProgress = false;
    m_capturedPulses.clear();
    m_status = QString("Saved signal '%1' to RCD '%2'").arg(sigName).arg(targetRcd);
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

void AppController::addSignalManually(const QString &name, const QString &protocol, int frequency, const QString &rawHex, const QVariantList &pulses, const QString &rcdName, const QString &shellCommand)
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

    QString targetRcd = rcdName.trimmed().isEmpty() ? "Default Remote Device" : rcdName.trimmed();

    ir::SavedIrSignal signal;
    signal.name = name.trimmed().toStdString();
    signal.rcdName = targetRcd.toStdString();
    signal.protocol = protocol.isEmpty() ? "Raw" : protocol.toStdString();
    signal.frequency_khz = static_cast<uint8_t>(frequency <= 0 ? 38 : frequency);
    signal.timestamp = QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss").toStdString();
    signal.rawHex = rawHex.trimmed().isEmpty() ? "N/A" : rawHex.trimmed().toStdString();
    signal.shellCommand = shellCommand.trimmed().toStdString();

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

    m_storage.addSignal(signal, targetRcd.toStdString());
    refreshSignals();
    m_status = QString("Saved signal '%1' to RCD '%2'").arg(name).arg(targetRcd);
    emit statusChanged();
}

void AppController::removeSavedSignal(int index)
{
    if (index < 0 || static_cast<size_t>(index) >= m_storage.count()) {
        m_status = "Invalid saved signal index";
        emit statusChanged();
        return;
    }

    if (m_storage.removeSignal(static_cast<size_t>(index))) {
        refreshSignals();
        m_status = "Saved signal removed";
        emit statusChanged();
    }
}

void AppController::startServer()
{
#if defined(Q_OS_ANDROID)
    m_status = "API server cannot run locally on Android. Please configure network API host in settings.";
    emit statusChanged();
    updateServerStatusDetails();
    return;
#else
    if (m_serverRunning) {
        m_status = "API server already running";
        emit statusChanged();
        updateServerStatusDetails();
        return;
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath("ir_api_server"),
        QDir::currentPath() + "/ir_api_server",
        QDir::cleanPath(QDir::currentPath() + "/build/debug/ir_api_server"),
        QDir::cleanPath(QDir::currentPath() + "/build/release/ir_api_server")
    };

    QString executablePath;
    for (const QString &candidate : candidates) {
        if (QFileInfo(candidate).isExecutable()) {
            executablePath = candidate;
            break;
        }
    }

    if (executablePath.isEmpty()) {
        m_status = "API server executable not found";
        emit statusChanged();
        updateServerStatusDetails();
        return;
    }

    const QString workingDir = QFileInfo(executablePath).absolutePath();
    const bool started = QProcess::startDetached(executablePath, QStringList(), workingDir);
    if (!started) {
        m_status = "Failed to start API server";
        emit statusChanged();
        updateServerStatusDetails();
        return;
    }

    m_status = "Starting API server...";
    emit statusChanged();
    updateServerStatusDetails();
#endif
}

void AppController::transmitSavedSignal(int index)
{
    if (index < 0 || static_cast<size_t>(index) >= m_storage.count()) {
        m_status = "Invalid saved signal index";
        emit statusChanged();
        return;
    }

    const auto signalOpt = m_storage.getSignal(static_cast<size_t>(index));
    if (!signalOpt.has_value()) {
        m_status = "Signal not found";
        emit statusChanged();
        return;
    }

    const auto& signal = signalOpt.value();
    const QUrl requestUrl = QUrl(m_apiUrl).resolved(QUrl("/api/v1/send"));
    QNetworkRequest request(requestUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body["frequency"] = static_cast<int>(signal.frequency_khz > 0 ? signal.frequency_khz : 38);

    if (!signal.pulses.empty()) {
        body["protocol"] = "Raw";
        QJsonArray pulsesArray;
        for (int p : signal.pulses) {
            pulsesArray.append(p);
        }
        body["pulses"] = pulsesArray;
    } else if (!signal.rawHex.empty() && signal.rawHex != "N/A") {
        body["protocol"] = QString::fromStdString(signal.protocol.empty() ? "NEC" : signal.protocol);
        body["code"] = QString::fromStdString(signal.rawHex);
    } else {
        body["name"] = QString::fromStdString(signal.name);
    }

    const QString signalName = QString::fromStdString(signal.name);
    m_status = QString("Transmitting '%1'...").arg(signalName);
    emit statusChanged();

    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    QNetworkReply *reply = m_networkManager->post(request, payload);
    connect(reply, &QNetworkReply::finished, this, [this, reply, signalName]() {
        reply->deleteLater();
        const QByteArray data = reply->readAll();
        if (reply->error() == QNetworkReply::NoError && !data.isEmpty()) {
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
            if (parseError.error == QJsonParseError::NoError && document.isObject()) {
                const QJsonObject object = document.object();
                if (object.value("success").toBool(false)) {
                    m_status = QString("Transmitted '%1'").arg(signalName);
                    emit statusChanged();
                    return;
                }
            }
        }

        QString errorDetail = reply->errorString();
        if (!data.isEmpty()) {
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
            if (parseError.error == QJsonParseError::NoError && document.isObject()) {
                const QString apiErr = document.object().value("error").toString();
                if (!apiErr.isEmpty()) {
                    errorDetail = apiErr;
                }
            }
        }
        m_status = QString("Transmit failed: %1").arg(errorDetail.isEmpty() ? "Unknown error" : errorDetail);
        emit statusChanged();
    });
}

void AppController::transmitSignal()
{
    m_status = "Transmit requested";
    emit statusChanged();
}

bool AppController::triggerModeActive() const
{
    return m_triggerModeActive;
}

int AppController::armedTriggerCount() const
{
    return m_armedTriggerCount;
}

QString AppController::lastTriggeredSignal() const
{
    return m_lastTriggeredSignal;
}

QString AppController::lastTriggeredCommand() const
{
    return m_lastTriggeredCommand;
}

QString AppController::triggerStatusText() const
{
    return m_triggerStatusText;
}

QStringList AppController::triggerLog() const
{
    return m_triggerLog;
}

QVariantList AppController::armedSignals() const
{
    QVariantList list;
    const auto& all = m_storage.getSignals();
    for (size_t i = 0; i < all.size(); ++i) {
        if (!all[i].shellCommand.empty()) {
            QVariantMap item;
            item["index"] = static_cast<int>(i);
            item["name"] = QString::fromStdString(all[i].name);
            item["rcdName"] = QString::fromStdString(all[i].rcdName);
            item["protocol"] = QString::fromStdString(all[i].protocol);
            item["frequency"] = static_cast<int>(all[i].frequency_khz);
            item["pulseCount"] = static_cast<int>(all[i].pulses.size());
            item["shellCommand"] = QString::fromStdString(all[i].shellCommand);
            list.append(item);
        }
    }
    return list;
}

void AppController::startTriggerMode()
{
    if (m_triggerModeActive) {
        return;
    }

    if (!m_serverConnected) {
        m_status = "Cannot start trigger mode: Server offline";
        m_triggerStatusText = m_status;
        emit statusChanged();
        emit triggerStatusTextChanged();
        return;
    }

    if (!m_irDeviceConnected) {
        m_status = "Cannot start trigger mode: IR hardware disconnected";
        m_triggerStatusText = m_status;
        emit statusChanged();
        emit triggerStatusTextChanged();
        return;
    }

    if (m_captureInProgress) {
        cancelCapture();
    }

    m_storage.loadFromFile();
    refreshSignals();

    const auto armedList = armedSignals();
    m_armedTriggerCount = armedList.size();
    emit armedSignalsChanged();
    emit armedTriggerCountChanged();

    if (m_armedTriggerCount == 0) {
        m_status = "No signals with shell commands configured";
        m_triggerStatusText = "No signals with shell commands found. Assign a shell command using Edit (✎) to arm it.";
        appendTriggerLog(QString("[%1] WARNING: No signals with configured shell commands found.").arg(QTime::currentTime().toString("HH:mm:ss")));
        emit statusChanged();
        emit triggerStatusTextChanged();
        return;
    }

    m_triggerModeActive = true;
    m_triggerStatusText = QString("Trigger Mode Active — Listening for %1 armed signal%2...")
                              .arg(m_armedTriggerCount)
                              .arg(m_armedTriggerCount == 1 ? "" : "s");
    m_status = m_triggerStatusText;
    appendTriggerLog(QString("[%1] Trigger Mode started. %2 armed trigger%3 waiting.")
                         .arg(QTime::currentTime().toString("HH:mm:ss"))
                         .arg(m_armedTriggerCount)
                         .arg(m_armedTriggerCount == 1 ? "" : "s"));

    emit triggerModeActiveChanged();
    emit triggerStatusTextChanged();
    emit statusChanged();

    pollNextTrigger();
}

void AppController::stopTriggerMode()
{
    if (!m_triggerModeActive) {
        return;
    }

    m_triggerModeActive = false;
    if (m_triggerReply) {
        m_triggerReply->abort();
        m_triggerReply = nullptr;
    }

    appendTriggerLog(QString("[%1] Trigger Mode stopped.").arg(QTime::currentTime().toString("HH:mm:ss")));
    m_triggerStatusText = "Trigger Mode stopped";
    m_status = "Trigger Mode stopped";

    emit triggerModeActiveChanged();
    emit triggerStatusTextChanged();
    emit statusChanged();
}

void AppController::toggleTriggerMode()
{
    if (m_triggerModeActive) {
        stopTriggerMode();
    } else {
        startTriggerMode();
    }
}

void AppController::clearTriggerLog()
{
    m_triggerLog.clear();
    emit triggerLogChanged();
}

void AppController::appendTriggerLog(const QString &entry)
{
    m_triggerLog.prepend(entry);
    while (m_triggerLog.size() > 100) {
        m_triggerLog.removeLast();
    }
    emit triggerLogChanged();
}

void AppController::pollNextTrigger()
{
    if (!m_triggerModeActive) {
        return;
    }

    if (!m_serverConnected || !m_irDeviceConnected) {
        appendTriggerLog(QString("[%1] ERROR: Server or USB IR device disconnected.").arg(QTime::currentTime().toString("HH:mm:ss")));
        m_triggerStatusText = "Device disconnected. Trigger mode stopped.";
        stopTriggerMode();
        return;
    }

    const QUrl requestUrl = QUrl(m_apiUrl).resolved(QUrl("/api/v1/capture"));
    QNetworkRequest request(requestUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body["timeout"] = 2.0;
    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    m_triggerReply = m_networkManager->post(request, payload);
    connect(m_triggerReply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply *reply = m_triggerReply;
        m_triggerReply = nullptr;

        if (!reply) {
            return;
        }

        const bool wasActive = m_triggerModeActive;
        const auto err = reply->error();
        const QByteArray data = reply->readAll();
        reply->deleteLater();

        if (!wasActive) {
            return;
        }

        if (err == QNetworkReply::OperationCanceledError) {
            return;
        }

        if (err == QNetworkReply::NoError && !data.isEmpty()) {
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
            if (parseError.error == QJsonParseError::NoError && document.isObject()) {
                const QJsonObject object = document.object();
                if (object.value("success").toBool(false) && object.contains("signal")) {
                    const QJsonObject signalObject = object.value("signal").toObject();
                    const QJsonArray pulseArray = signalObject.value("pulses").toArray();
                    ir::IrSignal sig;
                    for (const auto &p : pulseArray) {
                        sig.pulses.push_back(p.toInt());
                    }
                    sig.frequency_khz = static_cast<uint8_t>(signalObject.value("frequency").toInt(38));
                    sig.timestamp = std::chrono::system_clock::now();

                    auto matched = m_storage.findMatchingSignal(sig);
                    if (matched && !matched->shellCommand.empty()) {
                        m_lastTriggeredSignal = QString::fromStdString(matched->name);
                        m_lastTriggeredCommand = QString::fromStdString(matched->shellCommand);
                        QString rcdName = QString::fromStdString(matched->rcdName);

                        emit lastTriggeredSignalChanged();
                        emit lastTriggeredCommandChanged();

                        appendTriggerLog(QString("[%1] TRIGGER DETECTED: \"%2\" [%3]")
                                             .arg(QTime::currentTime().toString("HH:mm:ss"))
                                             .arg(m_lastTriggeredSignal)
                                             .arg(rcdName));
                        appendTriggerLog(QString("[%1] EXECUTING COMMAND: %2")
                                             .arg(QTime::currentTime().toString("HH:mm:ss"))
                                             .arg(m_lastTriggeredCommand));

#if defined(_WIN32)
                        QProcess::startDetached("cmd.exe", QStringList() << "/c" << m_lastTriggeredCommand);
#elif defined(Q_OS_ANDROID)
                        QProcess::startDetached("/system/bin/sh", QStringList() << "-c" << m_lastTriggeredCommand);
#else
                        QProcess::startDetached("/bin/sh", QStringList() << "-c" << m_lastTriggeredCommand);
#endif

                        m_triggerStatusText = QString("Executed: %1. Waiting for next trigger...")
                                                  .arg(m_lastTriggeredCommand);
                        m_status = m_triggerStatusText;
                        emit triggerStatusTextChanged();
                        emit statusChanged();

                        // Debounce delay to prevent burst repeat triggers before listening for the next command
                        QTimer::singleShot(350, this, &AppController::pollNextTrigger);
                        return;
                    } else if (matched) {
                        appendTriggerLog(QString("[%1] Ignored: \"%2\" [%3] (no shell command configured)")
                                             .arg(QTime::currentTime().toString("HH:mm:ss"))
                                             .arg(QString::fromStdString(matched->name))
                                             .arg(QString::fromStdString(matched->rcdName)));
                    } else {
                        appendTriggerLog(QString("[%1] Ignored: IR signal (%2 pulses) does not match any armed trigger")
                                             .arg(QTime::currentTime().toString("HH:mm:ss"))
                                             .arg(sig.pulses.size()));
                    }
                }
            }
            pollNextTrigger();
            return;
        }

        if (m_triggerModeActive) {
            int retryDelay = (err == QNetworkReply::TimeoutError || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 408) ? 50 : 500;
            QTimer::singleShot(retryDelay, this, &AppController::pollNextTrigger);
        }
    });
}

