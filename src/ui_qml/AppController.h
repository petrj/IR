#pragma once

#include "SignalStorage.h"

#include <QObject>
#include <QString>
#include <QStringList>
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
    Q_PROPERTY(QString usbStatusDetails READ usbStatusDetails NOTIFY usbStatusDetailsChanged)
    Q_PROPERTY(QString captureName READ captureName WRITE setCaptureName NOTIFY captureNameChanged)
    Q_PROPERTY(QString captureRcdName READ captureRcdName WRITE setCaptureRcdName NOTIFY captureRcdNameChanged)
    Q_PROPERTY(QStringList rcdList READ rcdList NOTIFY rcdListChanged)
    Q_PROPERTY(QString selectedDeviceFilter READ selectedDeviceFilter WRITE setSelectedDeviceFilter NOTIFY selectedDeviceFilterChanged)

public:
    explicit AppController(QObject *parent = nullptr);

    QString status() const;
    QString apiUrl() const;
    bool serverConnected() const;
    bool captureInProgress() const;
    bool captureReady() const;
    QString captureName() const;
    QString captureRcdName() const;
    QStringList rcdList() const;
    QString selectedDeviceFilter() const;
    bool irDeviceConnected() const;
    QString deviceState() const;
    QString usbStatusDetails() const;
    QString apiHost() const;
    bool serverRunning() const;
    QVariantList savedSignals() const;
    void setApiUrl(const QString &url);
    void setCaptureName(const QString &name);
    void setCaptureRcdName(const QString &name);
    void setSelectedDeviceFilter(const QString &filter);

public slots:
    void refreshStatus();
    void refreshSignals();
    void startCapture();
    Q_INVOKABLE QVariantMap getSignalDetails(int index) const;
    Q_INVOKABLE void addSignalManually(const QString &name, const QString &protocol, int frequency, const QString &rawHex, const QVariantList &pulses, const QString &rcdName = QString(), const QString &shellCommand = QString());
    Q_INVOKABLE void updateSignal(int index, const QString &name, const QString &protocol, int frequency, const QString &rawHex, const QVariantList &pulses, const QString &rcdName = QString(), const QString &shellCommand = QString());
    Q_INVOKABLE void removeSavedSignal(int index);
    Q_INVOKABLE void startServer();
    Q_INVOKABLE void transmitSavedSignal(int index);
    Q_INVOKABLE void refreshUsbStatusDialog();
    Q_INVOKABLE void saveCapturedSignal(const QString &rcdName = QString(), const QString &signalName = QString(), const QString &shellCommand = QString());
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
    void usbStatusDetailsChanged();
    void apiHostChanged();
    void serverRunningChanged();
    void savedSignalsChanged();
    void captureNameChanged();
    void captureRcdNameChanged();
    void rcdListChanged();
    void selectedDeviceFilterChanged();

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QString m_status;
    QString m_apiUrl;
    bool m_serverConnected;
    bool m_captureInProgress;
    bool m_captureReady;
    QString m_captureName;
    QString m_captureRcdName;
    QString m_selectedDeviceFilter{"All Devices"};
    QVector<int> m_capturedPulses;
    int m_capturedFrequency;
    QNetworkAccessManager *m_networkManager;
    QTimer *m_refreshTimer;
    bool m_irDeviceConnected;
    QString m_deviceState;
    QString m_usbStatusDetails;
    QString m_apiHost;
    bool m_serverRunning;
    QVariantList m_signals;
    ir::SignalStorage m_storage;
};
