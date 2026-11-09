/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QDBusInterface>
#include <QObject>
#include <QSet>
#include <QString>
#include <core/kdeconnectplugin.h>

#define PACKET_TYPE_PANIC_SHIELD QStringLiteral("kdeconnect.panicshield")

class PanicShieldPlugin : public KdeConnectPlugin
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeconnect.device.panicshield")
    Q_PROPERTY(bool isShieldActive READ isShieldActive NOTIFY shieldActiveChanged)

public:
    explicit PanicShieldPlugin(QObject *parent, const QVariantList &args);
    ~PanicShieldPlugin() override;

    Q_SCRIPTABLE void triggerPanicShield();
    Q_SCRIPTABLE void restoreNormalState();
    Q_SCRIPTABLE void togglePanicShield();
    Q_SCRIPTABLE void emergencyLock();
    Q_SCRIPTABLE bool isShieldActive() const;

    QString dbusPath() const override;
    void receivePacket(const NetworkPacket &np) override;
    void connected() override;

Q_SIGNALS:
    Q_SCRIPTABLE void shieldActiveChanged(bool active);

private Q_SLOTS:
    void sendState();
    void configChanged();

private:
    void applyShield();
    void releaseShield();

    bool m_isActive;
    bool m_desktopHidden;
    uint m_inhibitCookie;
    QSet<QString> m_mutedSinks;
    QSet<QString> m_pausedPlayers;

    // Config options
    bool m_optHideDesktop;
    bool m_optMuteAudio;
    bool m_optPauseMedia;
    bool m_optInhibitNotifications;
    bool m_optLockSession;
};
