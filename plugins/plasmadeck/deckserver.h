/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>

class DeckServer : public QObject
{
    Q_OBJECT

public:
    explicit DeckServer(quint16 port = 1739, QObject *parent = nullptr);
    ~DeckServer() override;

    bool start();
    void stop();
    quint16 port() const;
    QString serverUrl(const QString &hostIp) const;

    QString authToken() const;
    void setAuthToken(const QString &token);

    QStringList allowedAddresses() const;
    void setAllowedAddresses(const QStringList &addresses);

    void broadcastTelemetry(int cpuPercent, int ramPercent, int tempC);
    void broadcastContext(const QString &appName, const QString &windowTitle, const QJsonArray &buttons);
    void broadcastDualKeyRequest(const QString &transactionId, const QString &service, const QString &command, const QString &riskLevel, int timeoutSec = 25);
    void broadcastDualKeyResolved(const QString &transactionId, bool approved);

Q_SIGNALS:
    void actionTriggered(const QString &actionId, const QString &command, const QString &shortcut);
    void spatialEventReceived(const QString &type, const QString &stateOrGesture, int durationMs);
    void flickReceived(const QString &fileName, const QString &fileDataB64, const QString &textContent, const QString &action);
    void dualKeyResponseReceived(const QString &transactionId, bool approved);

private Q_SLOTS:
    void onNewConnection();
    void onClientDataReady();
    void onClientDisconnected();

private:
    void handleHttpRequest(QTcpSocket *socket, const QByteArray &data);
    void sendResponse(QTcpSocket *socket, int statusCode, const QString &contentType, const QByteArray &body, const QString &origin = QString());
    bool isRequestAuthorized(QTcpSocket *socket, const QString &path, const QString &reqHeaders, const QByteArray &body) const;
    bool isClientIpAllowed(const QHostAddress &addr) const;
    QByteArray getIndexHtml() const;
    QByteArray getManifestJson() const;
    QByteArray getIconSvg() const;

    QTcpServer m_server;
    quint16 m_port;
    QList<QTcpSocket *> m_sseClients;
    QMap<QTcpSocket *, QByteArray> m_clientBuffers;

    QString m_authToken;
    QStringList m_allowedAddresses;

    int m_lastCpu = 0;
    int m_lastRam = 0;
    int m_lastTemp = 0;
    QString m_lastAppName;
    QString m_lastWindowTitle;
    QJsonArray m_lastButtons;
    QJsonObject m_pendingDualKey;
};
