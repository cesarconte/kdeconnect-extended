/**
 * SPDX-FileCopyrightText: 2015 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "lockdeviceplugin.h"

#include <KLocalizedString>
#include <KPluginFactory>

#include "plugin_lockdevice_debug.h"
#include <QDebug>

#include <core/daemon.h>
#include <core/device.h>
#include <dbushelper.h>

K_PLUGIN_CLASS_WITH_JSON(LockDevicePlugin, "kdeconnect_lockdevice.json")

LockDevicePlugin::LockDevicePlugin(QObject *parent, const QVariantList &args)
    : KdeConnectPlugin(parent, args)
    , m_remoteLocked(false)
    , m_login1Interface(QStringLiteral("org.freedesktop.login1"), QStringLiteral("/org/freedesktop/login1/session/auto"), QDBusConnection::systemBus())
    // Connect on all paths since the PropertiesChanged signal is only emitted
    // from /org/freedesktop/login1/session/<sessionId> and not /org/freedesktop/login1/session/auto
    , m_propertiesInterface(QStringLiteral("org.freedesktop.login1"), QString(), QDBusConnection::systemBus())
{
    if (!m_login1Interface.isValid()) {
        qCWarning(KDECONNECT_PLUGIN_LOCKDEVICE) << "Could not connect to logind interface" << m_login1Interface.lastError();
    }

    if (!m_propertiesInterface.isValid()) {
        qCWarning(KDECONNECT_PLUGIN_LOCKDEVICE) << "Could not connect to logind properties interface" << m_propertiesInterface.lastError();
    }

    connect(&m_propertiesInterface,
            &OrgFreedesktopDBusPropertiesInterface::PropertiesChanged,
            this,
            [this](const QString &interface, const QVariantMap &properties) {
                if (interface != QLatin1String("org.freedesktop.login1.Session")) {
                    return;
                }

                if (!properties.contains(QStringLiteral("LockedHint"))) {
                    return;
                }

                m_localLocked = properties.value(QStringLiteral("LockedHint")).toBool();
                sendState();
            });

    m_localLocked = m_login1Interface.lockedHint();

    m_walkAwayTimer.setSingleShot(true);
    m_walkAwayTimer.setInterval(15000); // 15 segundos de gracia al alejarse
    connect(&m_walkAwayTimer, &QTimer::timeout, this, [this]() {
        if (isWalkAwayLockEnabled() && !device()->isReachable() && !m_localLocked) {
            qCDebug(KDECONNECT_PLUGIN_LOCKDEVICE) << "Walk-away threshold exceeded, locking session";
            m_login1Interface.Lock();
        }
    });

    connect(device(), &Device::reachableChanged, this, [this](bool reachable) {
        if (!reachable) {
            if (isWalkAwayLockEnabled() && !m_localLocked) {
                m_walkAwayTimer.start();
            }
        } else {
            m_walkAwayTimer.stop();
            if (isAutoUnlockEnabled() && m_localLocked) {
                initiateProximityChallenge();
            }
        }
    });
}

bool LockDevicePlugin::isAutoUnlockEnabled() const
{
    return config()->getBool(QStringLiteral("autoUnlockOnProximity"), false);
}

void LockDevicePlugin::setAutoUnlockEnabled(bool enabled)
{
    config()->set(QStringLiteral("autoUnlockOnProximity"), enabled);
    Q_EMIT autoUnlockChanged(enabled);
}

bool LockDevicePlugin::isWalkAwayLockEnabled() const
{
    return config()->getBool(QStringLiteral("lockOnDisconnect"), false);
}

void LockDevicePlugin::setWalkAwayLockEnabled(bool enabled)
{
    config()->set(QStringLiteral("lockOnDisconnect"), enabled);
    Q_EMIT walkAwayLockChanged(enabled);
}

void LockDevicePlugin::initiateProximityChallenge()
{
    if (!device()->isReachable()) {
        return;
    }

    const quint64 randomVal = QRandomGenerator::securelySeeded().generate64();
    m_currentChallengeNonce = QByteArray::number(randomVal, 16);
    m_challengeTimestamp = QDateTime::currentMSecsSinceEpoch();

    NetworkPacket np(PACKET_TYPE_LOCK_REQUEST);
    np.set(QStringLiteral("proximityChallenge"), m_currentChallengeNonce);
    sendPacket(np);
    qCDebug(KDECONNECT_PLUGIN_LOCKDEVICE) << "Dispatched proximity unlock challenge to" << device()->name();
}

void LockDevicePlugin::handleProximityChallenge(const NetworkPacket &np)
{
    const QByteArray nonce = np.get<QByteArray>(QStringLiteral("proximityChallenge"));
    if (nonce.isEmpty()) {
        return;
    }

    QByteArray payload = nonce;
    payload.append(device()->id().toUtf8());
    const QByteArray expectedResponse = QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex();
    NetworkPacket resp(PACKET_TYPE_LOCK);
    resp.set(QStringLiteral("proximityResponse"), expectedResponse);
    sendPacket(resp);
}

void LockDevicePlugin::handleProximityResponse(const NetworkPacket &np)
{
    const QByteArray response = np.get<QByteArray>(QStringLiteral("proximityResponse"));
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    // Comprobar frescura del nonce (menos de 5 segundos)
    if (m_currentChallengeNonce.isEmpty() || (now - m_challengeTimestamp > 5000)) {
        qCWarning(KDECONNECT_PLUGIN_LOCKDEVICE) << "Stale or missing proximity challenge nonce, ignoring";
        m_currentChallengeNonce.clear();
        return;
    }

    QByteArray payload = m_currentChallengeNonce;
    payload.append(device()->id().toUtf8());
    const QByteArray expectedResponse = QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex();
    m_currentChallengeNonce.clear();

    if (response == expectedResponse) {
        qCDebug(KDECONNECT_PLUGIN_LOCKDEVICE) << "Proximity cryptographic challenge verified! Unlocking screen.";
        m_login1Interface.Unlock();
        Daemon::instance()->sendSimpleNotification(QStringLiteral("proximityUnlockSuccess"),
                                                   device()->name(),
                                                   i18n("Sesión desbloqueada vía KDE Connect"),
                                                   QStringLiteral("unlock"));
    } else {
        qCWarning(KDECONNECT_PLUGIN_LOCKDEVICE) << "Proximity challenge verification failed: invalid cryptographic proof";
    }
}

bool LockDevicePlugin::isLocked() const
{
    return m_remoteLocked;
}

void LockDevicePlugin::setLocked(bool locked)
{
    NetworkPacket np(PACKET_TYPE_LOCK_REQUEST, {{QStringLiteral("setLocked"), locked}});
    sendPacket(np);
}

void LockDevicePlugin::receivePacket(const NetworkPacket &np)
{
    if (np.has(QStringLiteral("isLocked"))) {
        bool locked = np.get<bool>(QStringLiteral("isLocked"));
        if (m_remoteLocked != locked) {
            m_remoteLocked = locked;
            Q_EMIT lockedChanged(locked);
        }
    }

    if (np.has(QStringLiteral("requestLocked"))) {
        sendState();
    }

    // Receiving result of setLocked
    if (np.has(QStringLiteral("lockResult"))) {
        bool lockSuccess = np.get<bool>(QStringLiteral("lockResult"));
        if (lockSuccess) {
            Daemon::instance()->sendSimpleNotification(QStringLiteral("remoteLockSuccess"),
                                                       device()->name(),
                                                       i18n("Remote lock successful"),
                                                       QStringLiteral("lock"));
        } else {
            Daemon::instance()->sendSimpleNotification(QStringLiteral("remoteLockFailure"),
                                                       device()->name(),
                                                       i18n("Remote lock failed"),
                                                       QStringLiteral("error"));
            Daemon::instance()->reportError(device()->name(), i18n("Remote lock failed"));
        }
    }

    if (np.has(QStringLiteral("proximityChallenge"))) {
        handleProximityChallenge(np);
    }

    if (np.has(QStringLiteral("proximityResponse"))) {
        handleProximityResponse(np);
    }

    if (np.has(QStringLiteral("setLocked"))) {
        const bool lock = np.get<bool>(QStringLiteral("setLocked"));
        bool success = false;
        if (lock) {
            m_login1Interface.Lock();
            success = m_login1Interface.lockedHint();
            NetworkPacket np(PACKET_TYPE_LOCK, {{QStringLiteral("lockResult"), success}});
            sendPacket(np);
        } else {
            m_login1Interface.Unlock();
        }

        sendState();
    }
}

void LockDevicePlugin::sendState()
{
    NetworkPacket np(PACKET_TYPE_LOCK, {{QStringLiteral("isLocked"), m_localLocked}});
    sendPacket(np);
}

void LockDevicePlugin::connected()
{
    NetworkPacket np(PACKET_TYPE_LOCK_REQUEST, {{QStringLiteral("requestLocked"), QVariant()}});
    sendPacket(np);

    if (isAutoUnlockEnabled() && m_localLocked) {
        initiateProximityChallenge();
    }
}

QString LockDevicePlugin::dbusPath() const
{
    return QLatin1String("/modules/kdeconnect/devices/%1/lockdevice").arg(device()->id());
}

#include "lockdeviceplugin.moc"
#include "moc_lockdeviceplugin.cpp"
