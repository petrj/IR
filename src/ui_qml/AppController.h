#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString apiUrl READ apiUrl WRITE setApiUrl NOTIFY apiUrlChanged)
    Q_PROPERTY(bool serverConnected READ serverConnected NOTIFY serverConnectedChanged)

public:
    explicit AppController(QObject *parent = nullptr);

    QString status() const;
    QString apiUrl() const;
    bool serverConnected() const;
    void setApiUrl(const QString &url);

public slots:
    void refreshStatus();
    void startCapture();
    void transmitSignal();

signals:
    void statusChanged();
    void apiUrlChanged();
    void serverConnectedChanged();

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QString m_status;
    QString m_apiUrl;
    bool m_serverConnected;
    QNetworkAccessManager *m_networkManager;
    QTimer *m_refreshTimer;
};
