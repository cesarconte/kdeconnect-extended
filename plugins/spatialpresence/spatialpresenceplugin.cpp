/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "spatialpresenceplugin.h"

#include <KLocalizedString>
#include <KNotification>
#include <KPluginFactory>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>

#include "plugin_spatialpresence_debug.h"

K_PLUGIN_CLASS_WITH_JSON(SpatialPresencePlugin, "kdeconnect_spatialpresence.json")

SpatialPresencePlugin::SpatialPresencePlugin(QObject *parent, const QVariantList &args)
    : KdeConnectPlugin(parent, args)
{
    m_dimTimer.setSingleShot(true);
    connect(&m_dimTimer, &QTimer::timeout, this, &SpatialPresencePlugin::onDimTimeout);

    m_lockTimer.setSingleShot(true);
    connect(&m_lockTimer, &QTimer::timeout, this, &SpatialPresencePlugin::onLockTimeout);

    connect(&m_fadeTimer, &QTimer::timeout, this, &SpatialPresencePlugin::onFadeStep);

    connect(config(), &KdeConnectPluginConfig::configChanged, this, &SpatialPresencePlugin::onPluginConfigChanged);
    onPluginConfigChanged();
}

SpatialPresencePlugin::~SpatialPresencePlugin()
{
    m_dimTimer.stop();
    m_lockTimer.stop();
    m_fadeTimer.stop();

    if (m_isDimmed && m_savedBrightness > 0) {
        setSystemBrightnessSilent(m_savedBrightness);
    }
}

void SpatialPresencePlugin::onPluginConfigChanged()
{
    m_optPresenceDimming = config()->getBool(QStringLiteral("presenceDimmingEnabled"), true);
    m_optPresenceLock = config()->getBool(QStringLiteral("presenceLockEnabled"), true);
    m_optWaveGestures = config()->getBool(QStringLiteral("waveGesturesEnabled"), true);
    m_optMicHoldGesture = config()->getBool(QStringLiteral("micHoldGestureEnabled"), true);
    m_optVolumeGestures = config()->getBool(QStringLiteral("volumeGesturesEnabled"), true);
    m_dimTimeoutSeconds = config()->getInt(QStringLiteral("dimTimeoutSeconds"), 15);
    m_lockTimeoutSeconds = config()->getInt(QStringLiteral("lockTimeoutSeconds"), 60);
    m_dimBrightnessPercent = config()->getInt(QStringLiteral("dimBrightnessPercent"), 20);

    Q_EMIT configChanged();
}

void SpatialPresencePlugin::connected()
{
    // Plugin connected and ready
}

QString SpatialPresencePlugin::dbusPath() const
{
    return QStringLiteral("/modules/kdeconnect/devices/%1/spatialpresence").arg(device()->id());
}

bool SpatialPresencePlugin::isDimmed() const
{
    return m_isDimmed;
}

bool SpatialPresencePlugin::presenceDimmingEnabled() const
{
    return m_optPresenceDimming;
}

void SpatialPresencePlugin::setPresenceDimmingEnabled(bool enabled)
{
    if (m_optPresenceDimming != enabled) {
        m_optPresenceDimming = enabled;
        config()->set(QStringLiteral("presenceDimmingEnabled"), enabled);
        if (!enabled && m_isDimmed) {
            restoreBrightness();
        }
        Q_EMIT configChanged();
    }
}

bool SpatialPresencePlugin::presenceLockEnabled() const
{
    return m_optPresenceLock;
}

void SpatialPresencePlugin::setPresenceLockEnabled(bool enabled)
{
    if (m_optPresenceLock != enabled) {
        m_optPresenceLock = enabled;
        config()->set(QStringLiteral("presenceLockEnabled"), enabled);
        if (!enabled) {
            m_lockTimer.stop();
        }
        Q_EMIT configChanged();
    }
}

bool SpatialPresencePlugin::waveGesturesEnabled() const
{
    return m_optWaveGestures;
}

void SpatialPresencePlugin::setWaveGesturesEnabled(bool enabled)
{
    if (m_optWaveGestures != enabled) {
        m_optWaveGestures = enabled;
        config()->set(QStringLiteral("waveGesturesEnabled"), enabled);
        Q_EMIT configChanged();
    }
}

bool SpatialPresencePlugin::micHoldGestureEnabled() const
{
    return m_optMicHoldGesture;
}

void SpatialPresencePlugin::setMicHoldGestureEnabled(bool enabled)
{
    if (m_optMicHoldGesture != enabled) {
        m_optMicHoldGesture = enabled;
        config()->set(QStringLiteral("micHoldGestureEnabled"), enabled);
        Q_EMIT configChanged();
    }
}

bool SpatialPresencePlugin::volumeGesturesEnabled() const
{
    return m_optVolumeGestures;
}

void SpatialPresencePlugin::setVolumeGesturesEnabled(bool enabled)
{
    if (m_optVolumeGestures != enabled) {
        m_optVolumeGestures = enabled;
        config()->set(QStringLiteral("volumeGesturesEnabled"), enabled);
        Q_EMIT configChanged();
    }
}

int SpatialPresencePlugin::dimTimeoutSeconds() const
{
    return m_dimTimeoutSeconds;
}

void SpatialPresencePlugin::setDimTimeoutSeconds(int seconds)
{
    if (m_dimTimeoutSeconds != seconds && seconds > 0) {
        m_dimTimeoutSeconds = seconds;
        config()->set(QStringLiteral("dimTimeoutSeconds"), seconds);
        Q_EMIT configChanged();
    }
}

int SpatialPresencePlugin::lockTimeoutSeconds() const
{
    return m_lockTimeoutSeconds;
}

void SpatialPresencePlugin::setLockTimeoutSeconds(int seconds)
{
    if (m_lockTimeoutSeconds != seconds && seconds > 0) {
        m_lockTimeoutSeconds = seconds;
        config()->set(QStringLiteral("lockTimeoutSeconds"), seconds);
        Q_EMIT configChanged();
    }
}

int SpatialPresencePlugin::dimBrightnessPercent() const
{
    return m_dimBrightnessPercent;
}

void SpatialPresencePlugin::setDimBrightnessPercent(int percent)
{
    if (m_dimBrightnessPercent != percent && percent >= 0 && percent <= 100) {
        m_dimBrightnessPercent = percent;
        config()->set(QStringLiteral("dimBrightnessPercent"), percent);
        Q_EMIT configChanged();
    }
}

void SpatialPresencePlugin::receivePacket(const NetworkPacket &np)
{
    if (np.type() != PACKET_TYPE_SPATIAL_PRESENCE) {
        return;
    }

    if (np.has(QStringLiteral("gesture"))) {
        const QString gesture = np.get<QString>(QStringLiteral("gesture"));
        onGestureDetected(gesture);
    }

    if (np.has(QStringLiteral("presence"))) {
        const QString state = np.get<QString>(QStringLiteral("presence"));
        const int durationMs = np.get<int>(QStringLiteral("durationMs"), 0);
        onPresenceChanged(state, durationMs);
    }
}

void SpatialPresencePlugin::onGestureDetected(const QString &gestureType)
{
    qCDebug(KDECONNECT_PLUGIN_SPATIALPRESENCE) << "Spatial gesture received:" << gestureType;

    if (gestureType == QLatin1String("pointing_right") || gestureType == QLatin1String("wave_right")) {
        if (m_optWaveGestures) {
            QDBusInterface kwin(QStringLiteral("org.kde.kglobalaccel"),
                                QStringLiteral("/component/kwin"),
                                QStringLiteral("org.kde.kglobalaccel.Component"),
                                QDBusConnection::sessionBus());
            kwin.call(QDBus::NoBlock, QStringLiteral("invokeShortcut"), QStringLiteral("Switch One Desktop to the Right"));
            Q_EMIT gestureTriggered(gestureType);
        }
    } else if (gestureType == QLatin1String("pointing_left") || gestureType == QLatin1String("wave_left")) {
        if (m_optWaveGestures) {
            QDBusInterface kwin(QStringLiteral("org.kde.kglobalaccel"),
                                QStringLiteral("/component/kwin"),
                                QStringLiteral("org.kde.kglobalaccel.Component"),
                                QDBusConnection::sessionBus());
            kwin.call(QDBus::NoBlock, QStringLiteral("invokeShortcut"), QStringLiteral("Switch One Desktop to the Left"));
            Q_EMIT gestureTriggered(gestureType);
        }
    } else if (gestureType == QLatin1String("thumb_up") || gestureType == QLatin1String("wave_up")) {
        if (m_optVolumeGestures) {
            QDBusInterface kmix(QStringLiteral("org.kde.kglobalaccel"),
                                QStringLiteral("/component/kmix"),
                                QStringLiteral("org.kde.kglobalaccel.Component"),
                                QDBusConnection::sessionBus());
            kmix.call(QDBus::NoBlock, QStringLiteral("invokeShortcut"), QStringLiteral("increase_volume"));
            Q_EMIT gestureTriggered(gestureType);
        }
    } else if (gestureType == QLatin1String("thumb_down") || gestureType == QLatin1String("wave_down")) {
        if (m_optVolumeGestures) {
            QDBusInterface kmix(QStringLiteral("org.kde.kglobalaccel"),
                                QStringLiteral("/component/kmix"),
                                QStringLiteral("org.kde.kglobalaccel.Component"),
                                QDBusConnection::sessionBus());
            kmix.call(QDBus::NoBlock, QStringLiteral("invokeShortcut"), QStringLiteral("decrease_volume"));
            Q_EMIT gestureTriggered(gestureType);
        }
    } else if (gestureType == QLatin1String("palm_open") || gestureType == QLatin1String("media_play_pause")) {
        // Control Multimedia: Pausar/Reanudar mpv, YouTube en navegador, Spotify vía MPRIS / mediacontrol
        QDBusInterface media(QStringLiteral("org.kde.kglobalaccel"),
                             QStringLiteral("/component/mediacontrol"),
                             QStringLiteral("org.kde.kglobalaccel.Component"),
                             QDBusConnection::sessionBus());
        media.call(QDBus::NoBlock, QStringLiteral("invokeShortcut"), QStringLiteral("playpausemedia"));
        Q_EMIT gestureTriggered(gestureType);
    } else if (gestureType == QLatin1String("fist_hold") || gestureType == QLatin1String("palm_hold") || gestureType == QLatin1String("mic_mute")) {
        if (m_optMicHoldGesture) {
            QDBusInterface kmix(QStringLiteral("org.kde.kglobalaccel"),
                                QStringLiteral("/component/kmix"),
                                QStringLiteral("org.kde.kglobalaccel.Component"),
                                QDBusConnection::sessionBus());
            kmix.call(QDBus::NoBlock, QStringLiteral("invokeShortcut"), QStringLiteral("mic_mute"));
            Q_EMIT gestureTriggered(gestureType);
        }
    }
}

void SpatialPresencePlugin::onPresenceChanged(const QString &state, int durationMs)
{
    qCDebug(KDECONNECT_PLUGIN_SPATIALPRESENCE) << "Spatial presence changed:" << state << "duration:" << durationMs;

    if (state == QLatin1String("present")) {
        m_dimTimer.stop();
        m_lockTimer.stop();

        if (m_isDimmed) {
            restoreBrightness();
        }
        Q_EMIT presenceChanged(true);
    } else if (state == QLatin1String("absent") || state == QLatin1String("looking_away")) {
        Q_EMIT presenceChanged(false);

        if (durationMs > 0) {
            if (m_optPresenceDimming && durationMs >= (m_dimTimeoutSeconds * 1000) && !m_isDimmed) {
                dimScreen();
            }
            if (m_optPresenceLock && durationMs >= (m_lockTimeoutSeconds * 1000)) {
                lockSession();
            }
        } else {
            // Iniciar temporizadores si aún no están corriendo
            if (m_optPresenceDimming && !m_isDimmed && !m_dimTimer.isActive()) {
                m_dimTimer.start(m_dimTimeoutSeconds * 1000);
            }
            if (m_optPresenceLock && !m_lockTimer.isActive()) {
                m_lockTimer.start(m_lockTimeoutSeconds * 1000);
            }
        }
    }
}

void SpatialPresencePlugin::onDimTimeout()
{
    if (m_optPresenceDimming && !m_isDimmed) {
        dimScreen();
    }
}

void SpatialPresencePlugin::onLockTimeout()
{
    if (m_optPresenceLock) {
        lockSession();
    }
}

void SpatialPresencePlugin::dimScreen()
{
    if (m_isDimmed) {
        return;
    }

    const int current = getSystemBrightness();
    if (current <= 0) {
        return;
    }

    m_savedBrightness = current;
    const int max = getMaxBrightness();
    const int target = qMax(100, (max * m_dimBrightnessPercent) / 100);

    qCDebug(KDECONNECT_PLUGIN_SPATIALPRESENCE) << "Dimming screen from" << current << "to target" << target;
    startFadeTo(target);

    m_isDimmed = true;
    Q_EMIT dimStateChanged(true);
}

void SpatialPresencePlugin::restoreBrightness()
{
    m_dimTimer.stop();
    m_lockTimer.stop();

    if (!m_isDimmed && !m_fadeTimer.isActive()) {
        return;
    }

    if (m_savedBrightness > 0) {
        qCDebug(KDECONNECT_PLUGIN_SPATIALPRESENCE) << "Restoring brightness to" << m_savedBrightness;
        startFadeTo(m_savedBrightness);
        m_savedBrightness = -1;
    }

    m_isDimmed = false;
    Q_EMIT dimStateChanged(false);
}

void SpatialPresencePlugin::lockSession()
{
    qCDebug(KDECONNECT_PLUGIN_SPATIALPRESENCE) << "Locking session due to spatial presence absence";

    QDBusInterface screensaver(QStringLiteral("org.freedesktop.ScreenSaver"),
                               QStringLiteral("/ScreenSaver"),
                               QStringLiteral("org.freedesktop.ScreenSaver"),
                               QDBusConnection::sessionBus());
    if (screensaver.isValid()) {
        screensaver.call(QDBus::NoBlock, QStringLiteral("Lock"));
    } else {
        QDBusInterface login1(QStringLiteral("org.freedesktop.login1"),
                              QStringLiteral("/org/freedesktop/login1/session/auto"),
                              QStringLiteral("org.freedesktop.login1.Session"),
                              QDBusConnection::systemBus());
        if (login1.isValid()) {
            login1.call(QDBus::NoBlock, QStringLiteral("Lock"));
        }
    }
}

int SpatialPresencePlugin::getSystemBrightness() const
{
    QDBusInterface solid(QStringLiteral("org.kde.Solid.PowerManagement"),
                         QStringLiteral("/org/kde/Solid/PowerManagement/Actions/BrightnessControl"),
                         QStringLiteral("org.kde.Solid.PowerManagement.Actions.BrightnessControl"),
                         QDBusConnection::sessionBus());
    if (solid.isValid()) {
        QDBusReply<int> reply = solid.call(QStringLiteral("brightness"));
        if (reply.isValid()) {
            return reply.value();
        }
    }
    return -1;
}

int SpatialPresencePlugin::getMaxBrightness() const
{
    QDBusInterface solid(QStringLiteral("org.kde.Solid.PowerManagement"),
                         QStringLiteral("/org/kde/Solid/PowerManagement/Actions/BrightnessControl"),
                         QStringLiteral("org.kde.Solid.PowerManagement.Actions.BrightnessControl"),
                         QDBusConnection::sessionBus());
    if (solid.isValid()) {
        QDBusReply<int> reply = solid.call(QStringLiteral("brightnessMax"));
        if (reply.isValid()) {
            return reply.value();
        }
    }
    return 10000;
}

void SpatialPresencePlugin::setSystemBrightnessSilent(int value)
{
    QDBusInterface solid(QStringLiteral("org.kde.Solid.PowerManagement"),
                         QStringLiteral("/org/kde/Solid/PowerManagement/Actions/BrightnessControl"),
                         QStringLiteral("org.kde.Solid.PowerManagement.Actions.BrightnessControl"),
                         QDBusConnection::sessionBus());
    if (solid.isValid()) {
        solid.call(QDBus::NoBlock, QStringLiteral("setBrightnessSilent"), value);
    }
}

void SpatialPresencePlugin::startFadeTo(int targetBrightness)
{
    m_fadeTimer.stop();
    m_fadeStartBrightness = getSystemBrightness();
    if (m_fadeStartBrightness < 0) {
        m_fadeStartBrightness = targetBrightness;
    }
    m_fadeTarget = targetBrightness;
    m_fadeStepCount = 0;
    m_fadeTotalSteps = 8;
    m_fadeTimer.start(35);
}

void SpatialPresencePlugin::onFadeStep()
{
    m_fadeStepCount++;
    if (m_fadeStepCount >= m_fadeTotalSteps) {
        setSystemBrightnessSilent(m_fadeTarget);
        m_fadeTimer.stop();
        return;
    }

    const double progress = static_cast<double>(m_fadeStepCount) / m_fadeTotalSteps;
    const int val = m_fadeStartBrightness + static_cast<int>(progress * (m_fadeTarget - m_fadeStartBrightness));
    setSystemBrightnessSilent(val);
}

#include "spatialpresenceplugin.moc"
