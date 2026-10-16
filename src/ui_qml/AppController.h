#pragma once

#include "SignalStorage.h"

#include <QObject>
#include <QString>
#include <QVector>
#include <QVariant>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString apiUrl READ apiUrl WRITE setApiUrl NOTIFY apiUrlChanged)
    Q_PROPERTY(QString apiHost READ apiHost NOTIFY apiHostChanged)
    Q_PROPERTY(bool serverConnected READ serverConnected NOTIFY serverConnectedChanged)
    Q_PROPERTY(bool serverRunning READ serverRunning NOTIFY serverRunningChanged)
    Q_PROPERTY(QVariantList savedSignals READ savedSignals NOTIFY savedSignalsChanged)
    Q_PROPERTY(bool irDeviceConnected READ irDeviceConnected NOTIFY irDeviceConnectedChanged)
    Q_PROPERTY(bool captureInProgress READ captureInProgress NOTIFY captureInProgressChanged)
    Q_PROPERTY(bool captureReady READ captureReady NOTIFY captureReadyChanged)
    Q_PROPERTY(QString deviceState READ deviceState NOTIFY deviceStateChanged)
    Q_PROPERTY(QString captureName READ captureName WRITE setCaptureName NOTIFY captureNameChanged)

public:
    explicit AppController(QObject *parent = nullptr);

    QString status() const;
    QString apiUrl() const;
    bool serverConnected() const;
    bool captureInProgress() const;
    bool captureReady() const;
    QString captureName() const;
    bool irDeviceConnected() const;
    QString deviceState() const;
    QString apiHost() const;
    bool serverRunning() const;
    QVariantList savedSignals() const;
    void setApiUrl(const QString &url);
    void setCaptureName(const QString &name);

public slots:
    void refreshStatus();
    void refreshSignals();
    void startCapture();
    Q_INVOKABLE void addSignalManually(const QString &name, const QString &protocol, int frequency, const QString &rawHex, const QVariantList &pulses);
    void saveCapturedSignal();
    void cancelCapture();
    void transmitSignal();

signals:
    void statusChanged();
    void apiUrlChanged();
    void serverConnectedChanged();
    void irDeviceConnectedChanged();
    void captureInProgressChanged();
    void captureReadyChanged();
    void deviceStateChanged();
    void apiHostChanged();
    void serverRunningChanged();
    void savedSignalsChanged();
    void captureNameChanged();

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QString m_status;
    QString m_apiUrl;
    bool m_serverConnected;
    bool m_captureInProgress;
    bool m_captureReady;
    QString m_captureName;
    QVector<int> m_capturedPulses;
    int m_capturedFrequency;
    QNetworkAccessManager *m_networkManager;
    QTimer *m_refreshTimer;
    bool m_irDeviceConnected;
    QString m_deviceState;
    QString m_apiHost;
    bool m_serverRunning;
    QVariantList m_signals;
    ir::SignalStorage m_storage;
};
