/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "smartduckingplugin.h"

#include <KLocalizedString>
#include <KPluginFactory>
#include <PulseAudioQt/Context>
#include <PulseAudioQt/Sink>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>
#include <QProcess>

#include "core/device.h"
#include "core/networkpacket.h"

K_PLUGIN_CLASS_WITH_JSON(SmartDuckingPlugin, "kdeconnect_smartducking.json")

SmartDuckingPlugin::SmartDuckingPlugin(QObject *parent, const QVariantList &args)
    : KdeConnectPlugin(parent, args)
{
    m_duckingLevel = config()->getInt(QStringLiteral("duckingLevel"), 15);
    m_rampDurationMs = config()->getInt(QStringLiteral("rampDuration"), 450);
    m_restoreDurationMs = config()->getInt(QStringLiteral("restoreDuration"), 550);
    m_pauseMpris = config()->getBool(QStringLiteral("pauseMpris"), true);
    m_enableVoiceIsolation = config()->getBool(QStringLiteral("voiceIsolation"), true);

    connect(&m_rampTimer, &QTimer::timeout, this, &SmartDuckingPlugin::onRampStep);
    connect(&m_restoreTimer, &QTimer::timeout, this, &SmartDuckingPlugin::onRestoreStep);
}

SmartDuckingPlugin::~SmartDuckingPlugin()
{
    if (m_isDucked) {
        startRestoring(0); // Restore instantly on exit/unload
    }
}

QString SmartDuckingPlugin::dbusPath() const
{
    return QLatin1String("/modules/kdeconnect/devices/%1/smartducking").arg(device()->id());
}

bool SmartDuckingPlugin::isDucked() const
{
    return m_isDucked;
}

int SmartDuckingPlugin::duckingLevel() const
{
    return m_duckingLevel;
}

void SmartDuckingPlugin::setDuckingLevel(int level)
{
    int clamped = qBound(0, level, 100);
    if (m_duckingLevel != clamped) {
        m_duckingLevel = clamped;
        config()->set(QStringLiteral("duckingLevel"), m_duckingLevel);
        Q_EMIT duckingLevelChanged(m_duckingLevel);
    }
}

bool SmartDuckingPlugin::voiceIsolationActive() const
{
    return m_voiceIsolationActive;
}

void SmartDuckingPlugin::triggerDucking(bool duck)
{
    if (duck) {
        startDucking(m_duckingLevel, m_rampDurationMs);
    } else {
        startRestoring(m_restoreDurationMs);
    }
}

void SmartDuckingPlugin::setDuckingPercentage(int percent)
{
    setDuckingLevel(percent);
}

void SmartDuckingPlugin::toggleVoiceIsolation(bool enable)
{
    m_enableVoiceIsolation = enable;
    config()->set(QStringLiteral("voiceIsolation"), enable);
    if (!enable && m_voiceIsolationActive) {
        restoreVoiceIsolationPreset();
    } else if (enable && m_isDucked && !m_voiceIsolationActive) {
        applyVoiceIsolationPreset();
    }
}

void SmartDuckingPlugin::receivePacket(const NetworkPacket &np)
{
    if (np.type() != PACKET_TYPE_TELEPHONY) {
        return;
    }

    if (np.get<bool>(QStringLiteral("isCancel"), false)) {
        startRestoring(m_restoreDurationMs);
        return;
    }

    const QString event = np.get<QString>(QStringLiteral("event"));
    if (event == QLatin1String("ringing")) {
        startDucking(m_duckingLevel, m_rampDurationMs);
    } else if (event == QLatin1String("talking")) {
        startDucking(m_duckingLevel, m_rampDurationMs);
        if (m_enableVoiceIsolation) {
            applyVoiceIsolationPreset();
        }
    } else if (event == QLatin1String("missedCall")) {
        startRestoring(m_restoreDurationMs);
    }
}

void SmartDuckingPlugin::startDucking(int targetPercent, int durationMs)
{
    m_restoreTimer.stop();

    auto *ctx = PulseAudioQt::Context::instance();
    if (!ctx) {
        return;
    }

    const auto sinks = ctx->sinks();
    if (sinks.isEmpty()) {
        return;
    }

    m_startRampVolumes.clear();
    m_targetRampVolumes.clear();

    for (auto *sink : sinks) {
        if (!m_originalVolumes.contains(sink->name())) {
            m_originalVolumes[sink->name()] = sink->volume();
        }

        qint64 origVol = m_originalVolumes.value(sink->name());
        qint64 currentVol = sink->volume();
        qint64 duckedVol = (origVol * targetPercent) / 100;

        m_startRampVolumes[sink->name()] = currentVol;
        m_targetRampVolumes[sink->name()] = duckedVol;
    }

    m_currentAnimDuration = qMax(50, durationMs);
    m_animElapsed.restart();
    m_rampTimer.start(16); // ~60fps smooth volume ramp

    if (m_pauseMpris && !m_isDucked) {
        pauseMprisPlayers();
    }

    if (!m_isDucked) {
        m_isDucked = true;
        Q_EMIT duckedChanged(true);
    }
}

void SmartDuckingPlugin::onRampStep()
{
    auto *ctx = PulseAudioQt::Context::instance();
    if (!ctx) {
        m_rampTimer.stop();
        return;
    }

    qreal t = qMin(1.0, (qreal)m_animElapsed.elapsed() / m_currentAnimDuration);
    // Smooth ease-out quad curve: 1 - (1 - t)^2
    qreal ease = 1.0 - (1.0 - t) * (1.0 - t);

    const auto sinks = ctx->sinks();
    for (auto *sink : sinks) {
        if (!m_startRampVolumes.contains(sink->name()) || !m_targetRampVolumes.contains(sink->name())) {
            continue;
        }

        qint64 start = m_startRampVolumes.value(sink->name());
        qint64 target = m_targetRampVolumes.value(sink->name());
        qint64 newVol = start + static_cast<qint64>((target - start) * ease);
        sink->setVolume(newVol);
    }

    if (t >= 1.0) {
        m_rampTimer.stop();
    }
}

void SmartDuckingPlugin::startRestoring(int durationMs)
{
    m_rampTimer.stop();

    if (m_enableVoiceIsolation && m_voiceIsolationActive) {
        restoreVoiceIsolationPreset();
    }

    if (m_pauseMpris) {
        resumeMprisPlayers();
    }

    auto *ctx = PulseAudioQt::Context::instance();
    if (!ctx) {
        m_originalVolumes.clear();
        m_isDucked = false;
        Q_EMIT duckedChanged(false);
        return;
    }

    const auto sinks = ctx->sinks();
    if (durationMs <= 0 || sinks.isEmpty() || m_originalVolumes.isEmpty()) {
        for (auto *sink : sinks) {
            if (m_originalVolumes.contains(sink->name())) {
                sink->setVolume(m_originalVolumes.value(sink->name()));
            }
        }
        m_originalVolumes.clear();
        m_isDucked = false;
        Q_EMIT duckedChanged(false);
        return;
    }

    m_startRampVolumes.clear();
    m_targetRampVolumes.clear();

    for (auto *sink : sinks) {
        if (m_originalVolumes.contains(sink->name())) {
            m_startRampVolumes[sink->name()] = sink->volume();
            m_targetRampVolumes[sink->name()] = m_originalVolumes.value(sink->name());
        }
    }

    m_currentAnimDuration = qMax(50, durationMs);
    m_animElapsed.restart();
    m_restoreTimer.start(16);

    m_isDucked = false;
    Q_EMIT duckedChanged(false);
}

void SmartDuckingPlugin::onRestoreStep()
{
    auto *ctx = PulseAudioQt::Context::instance();
    if (!ctx) {
        m_restoreTimer.stop();
        m_originalVolumes.clear();
        return;
    }

    qreal t = qMin(1.0, (qreal)m_animElapsed.elapsed() / m_currentAnimDuration);
    // Smooth ease-out quad curve
    qreal ease = 1.0 - (1.0 - t) * (1.0 - t);

    const auto sinks = ctx->sinks();
    for (auto *sink : sinks) {
        if (!m_startRampVolumes.contains(sink->name()) || !m_targetRampVolumes.contains(sink->name())) {
            continue;
        }

        qint64 start = m_startRampVolumes.value(sink->name());
        qint64 target = m_targetRampVolumes.value(sink->name());
        qint64 newVol = start + static_cast<qint64>((target - start) * ease);
        sink->setVolume(newVol);
    }

    if (t >= 1.0) {
        m_restoreTimer.stop();
        m_originalVolumes.clear();
    }
}

void SmartDuckingPlugin::pauseMprisPlayers()
{
    m_pausedMprisPlayers.clear();
    const QStringList services = QDBusConnection::sessionBus().interface()->registeredServiceNames().value();
    for (const QString &service : services) {
        if (!service.startsWith(QStringLiteral("org.mpris.MediaPlayer2.")) || service.contains(QStringLiteral("kdeconnect"))) {
            continue;
        }

        QDBusInterface player(service,
                              QStringLiteral("/org/mpris/MediaPlayer2"),
                              QStringLiteral("org.mpris.MediaPlayer2.Player"),
                              QDBusConnection::sessionBus());
        if (player.isValid() && player.property("PlaybackStatus").toString() == QLatin1String("Playing")) {
            m_pausedMprisPlayers.insert(service);
            player.call(QDBus::NoBlock, QStringLiteral("Pause"));
        }
    }
}

void SmartDuckingPlugin::resumeMprisPlayers()
{
    for (const QString &service : std::as_const(m_pausedMprisPlayers)) {
        QDBusInterface player(service,
                              QStringLiteral("/org/mpris/MediaPlayer2"),
                              QStringLiteral("org.mpris.MediaPlayer2.Player"),
                              QDBusConnection::sessionBus());
        if (player.isValid()) {
            player.call(QDBus::NoBlock, QStringLiteral("Play"));
        }
    }
    m_pausedMprisPlayers.clear();
}

void SmartDuckingPlugin::applyVoiceIsolationPreset()
{
    if (m_voiceIsolationActive) {
        return;
    }

    // Capture current EasyEffects preset
    QProcess p;
    p.start(QStringLiteral("easyeffects"), {QStringLiteral("-s")});
    if (p.waitForFinished(1000)) {
        const QString out = QString::fromUtf8(p.readAllStandardOutput());
        const QStringList lines = out.split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            if (line.trimmed().startsWith(QStringLiteral("output:"))) {
                m_savedEasyEffectsPreset = line.section(QLatin1String("output:"), 1).trimmed();
                break;
            }
        }
    }

    // Apply voice isolation / vocal podcast profile
    QProcess::startDetached(QStringLiteral("easyeffects"), {QStringLiteral("-l"), QStringLiteral("Vocal-Podcast_TRUE_ULTRA_XB13")});
    m_voiceIsolationActive = true;
    Q_EMIT voiceIsolationChanged(true);
}

void SmartDuckingPlugin::restoreVoiceIsolationPreset()
{
    if (!m_voiceIsolationActive) {
        return;
    }

    if (!m_savedEasyEffectsPreset.isEmpty()) {
        QProcess::startDetached(QStringLiteral("easyeffects"), {QStringLiteral("-l"), m_savedEasyEffectsPreset});
        m_savedEasyEffectsPreset.clear();
    }

    m_voiceIsolationActive = false;
    Q_EMIT voiceIsolationChanged(false);
}

#include "moc_smartduckingplugin.cpp"
#include "smartduckingplugin.moc"
