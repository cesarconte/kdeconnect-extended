/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QDateTime>
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QString>
#include <QTimer>
#include <core/kdeconnectplugin.h>

#define PACKET_TYPE_BIOMETRIC_REQUEST QStringLiteral("kdeconnect.biometric.request")
#define PACKET_TYPE_BIOMETRIC_RESPONSE QStringLiteral("kdeconnect.biometric.response")
#define PACKET_TYPE_PING QStringLiteral("kdeconnect.ping")

class BiometricAuthPlugin : public KdeConnectPlugin
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeconnect.device.biometricauth")
    Q_PROPERTY(bool isPending READ isPending NOTIFY pendingChanged)
    Q_PROPERTY(QString lastPrompt READ lastPrompt NOTIFY pendingChanged)
    Q_PROPERTY(QString currentCommand READ currentCommand NOTIFY pendingChanged)
    Q_PROPERTY(QString currentService READ currentService NOTIFY pendingChanged)
    Q_PROPERTY(QString currentRiskLevel READ currentRiskLevel NOTIFY pendingChanged)
    Q_PROPERTY(QString currentTransactionId READ currentTransactionId NOTIFY pendingChanged)

public:
    BiometricAuthPlugin(QObject *parent, const QVariantList &args);
    ~BiometricAuthPlugin() override;

    bool isPending() const;
    QString lastPrompt() const;
    QString currentCommand() const;
    QString currentService() const;
    QString currentRiskLevel() const;
    QString currentTransactionId() const;

    Q_SCRIPTABLE void respondBiometric(bool accepted);
    Q_SCRIPTABLE void cancelBiometric();
    Q_SCRIPTABLE void requestBiometric(const QString &service);
    Q_SCRIPTABLE QString requestAuthorization(const QString &service, const QString &command, const QString &riskLevel = QStringLiteral("medium"));

    void receivePacket(const NetworkPacket &np) override;
    void connected() override;
    QString dbusPath() const override;

Q_SIGNALS:
    Q_SCRIPTABLE void pendingChanged();
    Q_SCRIPTABLE void authenticationFinished(bool success);
    Q_SCRIPTABLE void authRequested(const QString &transactionId, const QString &service, const QString &command, const QString &riskLevel);

private Q_SLOTS:
    void handleNewConnection();
    void handleSocketData();
    void handleAuthTimeout();

private:
    void setupSocketServer();
    void sendResponseToPam(const QString &status, const QString &message = QString());
    void notifyDesktop(const QString &service, const QString &command, const QString &riskLevel);
    void sendMobileNotification(const QString &service, const QString &command, const QString &riskLevel);
    void cancelMobileNotification();
    void registerRunCommands();
    QString calculateRiskLevel(const QString &command) const;

    QLocalServer *m_server = nullptr;
    QLocalSocket *m_currentSocket = nullptr;
    QTimer m_timeoutTimer;

    bool m_isPending = false;
    QString m_lastPrompt;
    QString m_currentTransactionId;
    QString m_currentCommand;
    QString m_currentService;
    QString m_currentRiskLevel;
};
