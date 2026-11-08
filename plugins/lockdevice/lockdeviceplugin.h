/**
 * SPDX-FileCopyrightText: 2015 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QByteArray>
#include <QCryptographicHash>
#include <QObject>
#include <QRandomGenerator>
#include <QTimer>

#include <core/kdeconnectplugin.h>

#include "generated/systeminterfaces/dbusproperties.h"
#include "generated/systeminterfaces/login1.h"

#define PACKET_TYPE_LOCK QStringLiteral("kdeconnect.lock")
#define PACKET_TYPE_LOCK_REQUEST QStringLiteral("kdeconnect.lock.request")

class LockDevicePlugin : public KdeConnectPlugin
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeconnect.device.lockdevice")
    Q_PROPERTY(bool isLocked READ isLocked WRITE setLocked NOTIFY lockedChanged)
    Q_PROPERTY(bool isAutoUnlockEnabled READ isAutoUnlockEnabled WRITE setAutoUnlockEnabled NOTIFY autoUnlockChanged)
    Q_PROPERTY(bool isWalkAwayLockEnabled READ isWalkAwayLockEnabled WRITE setWalkAwayLockEnabled NOTIFY walkAwayLockChanged)

public:
    explicit LockDevicePlugin(QObject *parent, const QVariantList &args);

    bool isLocked() const;
    Q_SCRIPTABLE void setLocked(bool);

    bool isAutoUnlockEnabled() const;
    Q_SCRIPTABLE void setAutoUnlockEnabled(bool enabled);

    bool isWalkAwayLockEnabled() const;
    Q_SCRIPTABLE void setWalkAwayLockEnabled(bool enabled);

    QString dbusPath() const override;
    void connected() override;
    void receivePacket(const NetworkPacket &np) override;

Q_SIGNALS:
    Q_SCRIPTABLE void lockedChanged(bool locked);
    Q_SCRIPTABLE void autoUnlockChanged(bool enabled);
    Q_SCRIPTABLE void walkAwayLockChanged(bool enabled);

private:
    void sendState();
    void initiateProximityChallenge();
    void handleProximityChallenge(const NetworkPacket &np);
    void handleProximityResponse(const NetworkPacket &np);

    bool m_remoteLocked;
    bool m_localLocked = false;

    QByteArray m_currentChallengeNonce;
    qint64 m_challengeTimestamp = 0;
    QTimer m_walkAwayTimer;

    OrgFreedesktopLogin1SessionInterface m_login1Interface;
    OrgFreedesktopDBusPropertiesInterface m_propertiesInterface;
};
