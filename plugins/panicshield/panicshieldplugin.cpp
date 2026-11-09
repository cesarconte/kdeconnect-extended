/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "panicshieldplugin.h"

#include <KLocalizedString>
#include <KNotification>
#include <KPluginFactory>
#include <PulseAudioQt/Context>
#include <PulseAudioQt/Sink>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>

#include "core/daemon.h"
#include "generated/systeminterfaces/mprisplayer.h"
#include "plugin_panicshield_debug.h"

K_PLUGIN_CLASS_WITH_JSON(PanicShieldPlugin, "kdeconnect_panicshield.json")

PanicShieldPlugin::PanicShieldPlugin(QObject *parent, const QVariantList &args)
    : KdeConnectPlugin(parent, args)
    , m_isActive(false)
    , m_desktopHidden(false)
    , m_inhibitCookie(0)
    , m_optHideDesktop(true)
    , m_optMuteAudio(true)
    , m_optPauseMedia(true)
    , m_optInhibitNotifications(true)
    , m_optLockSession(false)
{
    connect(config(), &KdeConnectPluginConfig::configChanged, this, &PanicShieldPlugin::configChanged);
    configChanged();
}

PanicShieldPlugin::~PanicShieldPlugin()
{
    if (m_isActive) {
        releaseShield();
    }
}

void PanicShieldPlugin::configChanged()
{
    m_optHideDesktop = config()->getBool(QStringLiteral("hideDesktop"), true);
    m_optMuteAudio = config()->getBool(QStringLiteral("muteAudio"), true);
    m_optPauseMedia = config()->getBool(QStringLiteral("pauseMedia"), true);
    m_optInhibitNotifications = config()->getBool(QStringLiteral("inhibitNotifications"), true);
    m_optLockSession = config()->getBool(QStringLiteral("lockSession"), false);
}

void PanicShieldPlugin::connected()
{
    sendState();
}

QString PanicShieldPlugin::dbusPath() const
{
    return QLatin1String("/modules/kdeconnect/devices/%1/panicshield").arg(device()->id());
}

bool PanicShieldPlugin::isShieldActive() const
{
    return m_isActive;
}

void PanicShieldPlugin::triggerPanicShield()
{
    if (!m_isActive) {
        applyShield();
    }
}

void PanicShieldPlugin::restoreNormalState()
{
    if (m_isActive) {
        releaseShield();
    }
}

void PanicShieldPlugin::togglePanicShield()
{
    if (m_isActive) {
        releaseShield();
    } else {
        applyShield();
    }
}

void PanicShieldPlugin::emergencyLock()
{
    qCDebug(KDECONNECT_PLUGIN_PANICSHIELD) << "Emergency lock triggered via Panic Shield";

    // 1. Systemd-logind session lock
    QDBusInterface login1(QStringLiteral("org.freedesktop.login1"),
                          QStringLiteral("/org/freedesktop/login1/session/auto"),
                          QStringLiteral("org.freedesktop.login1.Session"),
                          QDBusConnection::systemBus());
    if (login1.isValid()) {
        login1.call(QDBus::NoBlock, QStringLiteral("Lock"));
    }

    // 2. FreeDesktop ScreenSaver lock (KWin / Plasma fallback)
    QDBusInterface screensaver(QStringLiteral("org.freedesktop.ScreenSaver"),
                               QStringLiteral("/ScreenSaver"),
                               QStringLiteral("org.freedesktop.ScreenSaver"),
                               QDBusConnection::sessionBus());
    if (screensaver.isValid()) {
        screensaver.call(QDBus::NoBlock, QStringLiteral("Lock"));
    }
}

void PanicShieldPlugin::applyShield()
{
    qCDebug(KDECONNECT_PLUGIN_PANICSHIELD) << "Activating Panic Shield";

    // 1. Hide Desktop / Boss Key (KWin Wayland)
    if (m_optHideDesktop) {
        QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kglobalaccel"),
                                                          QStringLiteral("/component/kwin"),
                                                          QStringLiteral("org.kde.kglobalaccel.Component"),
                                                          QStringLiteral("invokeShortcut"));
        msg << QStringLiteral("Show Desktop");
        QDBusConnection::sessionBus().send(msg);
        m_desktopHidden = true;
    }

    // 2. Mute Audio Sinks (PipeWire / PulseAudio)
    if (m_optMuteAudio) {
        m_mutedSinks.clear();
        const auto sinks = PulseAudioQt::Context::instance()->sinks();
        for (const auto sink : sinks) {
            if (!sink->isMuted()) {
                sink->setMuted(true);
                m_mutedSinks.insert(sink->name());
            }
        }
    }

    // 3. Pause Active Media Players (MPRIS)
    if (m_optPauseMedia) {
        m_pausedPlayers.clear();
        const QStringList interfaces = QDBusConnection::sessionBus().interface()->registeredServiceNames().value();
        for (const QString &iface : interfaces) {
            if (!iface.startsWith(QLatin1String("org.mpris.MediaPlayer2."))) {
                continue;
            }
            if (iface.startsWith(QLatin1String("org.mpris.MediaPlayer2.kdeconnect."))) {
                continue;
            }
            OrgMprisMediaPlayer2PlayerInterface mprisInterface(iface, QStringLiteral("/org/mpris/MediaPlayer2"), QDBusConnection::sessionBus());
            const QString status = mprisInterface.playbackStatus();
            if (status == QLatin1String("Playing")) {
                m_pausedPlayers.insert(iface);
                if (mprisInterface.canPause()) {
                    mprisInterface.Pause();
                } else {
                    mprisInterface.Stop();
                }
            }
        }
    }

    // 4. Inhibit Desktop Notifications (Do Not Disturb)
    if (m_optInhibitNotifications) {
        QDBusInterface notifInterface(QStringLiteral("org.freedesktop.Notifications"),
                                      QStringLiteral("/org/freedesktop/Notifications"),
                                      QStringLiteral("org.freedesktop.Notifications"),
                                      QDBusConnection::sessionBus());
        if (notifInterface.isValid()) {
            QDBusReply<uint> reply =
                notifInterface.call(QStringLiteral("Inhibit"), QStringLiteral("kdeconnect_panicshield"), QStringLiteral("Panic Shield Active"), QVariantMap());
            if (reply.isValid()) {
                m_inhibitCookie = reply.value();
            }
        }
    }

    // 5. Optional Screen Lock
    if (m_optLockSession) {
        emergencyLock();
    }

    m_isActive = true;
    Q_EMIT shieldActiveChanged(true);
    sendState();
}

void PanicShieldPlugin::releaseShield()
{
    qCDebug(KDECONNECT_PLUGIN_PANICSHIELD) << "Deactivating Panic Shield, restoring normal state";

    // 1. Restore Windows
    if (m_desktopHidden) {
        QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kglobalaccel"),
                                                          QStringLiteral("/component/kwin"),
                                                          QStringLiteral("org.kde.kglobalaccel.Component"),
                                                          QStringLiteral("invokeShortcut"));
        msg << QStringLiteral("Show Desktop");
        QDBusConnection::sessionBus().send(msg);
        m_desktopHidden = false;
    }

    // 2. Unmute Audio Sinks
    if (!m_mutedSinks.isEmpty()) {
        const auto sinks = PulseAudioQt::Context::instance()->sinks();
        for (const auto sink : sinks) {
            if (m_mutedSinks.contains(sink->name())) {
                sink->setMuted(false);
            }
        }
        m_mutedSinks.clear();
    }

    // 3. Resume Media Playback
    if (!m_pausedPlayers.isEmpty()) {
        for (const QString &iface : std::as_const(m_pausedPlayers)) {
            OrgMprisMediaPlayer2PlayerInterface mprisInterface(iface, QStringLiteral("/org/mpris/MediaPlayer2"), QDBusConnection::sessionBus());
            mprisInterface.Play();
        }
        m_pausedPlayers.clear();
    }

    // 4. Un-inhibit Notifications
    if (m_inhibitCookie > 0) {
        QDBusInterface notifInterface(QStringLiteral("org.freedesktop.Notifications"),
                                      QStringLiteral("/org/freedesktop/Notifications"),
                                      QStringLiteral("org.freedesktop.Notifications"),
                                      QDBusConnection::sessionBus());
        if (notifInterface.isValid()) {
            notifInterface.call(QDBus::NoBlock, QStringLiteral("UnInhibit"), m_inhibitCookie);
        }
        m_inhibitCookie = 0;
    }

    m_isActive = false;
    Q_EMIT shieldActiveChanged(false);
    sendState();
}

void PanicShieldPlugin::sendState()
{
    NetworkPacket np(PACKET_TYPE_PANIC_SHIELD, {{QStringLiteral("isActive"), m_isActive}});
    sendPacket(np);
}

void PanicShieldPlugin::receivePacket(const NetworkPacket &np)
{
    if (np.has(QStringLiteral("action"))) {
        const QString action = np.get<QString>(QStringLiteral("action"));
        if (action == QLatin1String("trigger")) {
            triggerPanicShield();
        } else if (action == QLatin1String("restore")) {
            restoreNormalState();
        } else if (action == QLatin1String("toggle")) {
            togglePanicShield();
        } else if (action == QLatin1String("lock")) {
            emergencyLock();
        } else if (action == QLatin1String("requestState")) {
            sendState();
        }
    }
}

#include "moc_panicshieldplugin.cpp"
#include "panicshieldplugin.moc"
