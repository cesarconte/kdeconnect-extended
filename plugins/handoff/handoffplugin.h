/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <core/kdeconnectplugin.h>

#define PACKET_TYPE_HANDOFF_ACTIVITY QStringLiteral("kdeconnect.handoff.activity")
#define PACKET_TYPE_HANDOFF_CLEAR QStringLiteral("kdeconnect.handoff.clear")
#define PACKET_TYPE_SHARE_REQUEST QStringLiteral("kdeconnect.share.request")

class HandoffPlugin : public KdeConnectPlugin
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeconnect.device.handoff")
    Q_PROPERTY(bool hasActiveActivity READ hasActiveActivity NOTIFY activityChanged)
    Q_PROPERTY(QString currentUrl READ currentUrl NOTIFY activityChanged)
    Q_PROPERTY(QString currentTitle READ currentTitle NOTIFY activityChanged)
    Q_PROPERTY(QString sourceApp READ sourceApp NOTIFY activityChanged)
    Q_PROPERTY(qint64 activityTimestamp READ activityTimestamp NOTIFY activityChanged)

public:
    using KdeConnectPlugin::KdeConnectPlugin;
    ~HandoffPlugin() override = default;

    bool hasActiveActivity() const;
    QString currentUrl() const;
    QString currentTitle() const;
    QString sourceApp() const;
    qint64 activityTimestamp() const;

    Q_SCRIPTABLE void openCurrentActivity();
    Q_SCRIPTABLE void broadcastActivity(const QString &url, const QString &title, const QString &sourceApp);
    Q_SCRIPTABLE void clearActivity();
    void setActivity(const QString &url, const QString &title, const QString &sourceApp);

    void receivePacket(const NetworkPacket &np) override;
    QString dbusPath() const override;

Q_SIGNALS:
    Q_SCRIPTABLE void activityChanged();
    Q_SCRIPTABLE void activityReceived(const QString &url, const QString &title, const QString &sourceApp);

private:
    QString m_currentUrl;
    QString m_currentTitle;
    QString m_sourceApp;
    qint64 m_activityTimestamp = 0;
    QTimer m_expiryTimer;
};
