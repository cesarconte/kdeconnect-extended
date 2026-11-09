/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include <core/kdeconnectplugin.h>

#define PACKET_TYPE_SPATIAL_PRESENCE QStringLiteral("kdeconnect.spatialpresence")

class SpatialPresencePlugin : public KdeConnectPlugin
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeconnect.device.spatialpresence")
    Q_PROPERTY(bool isDimmed READ isDimmed NOTIFY dimStateChanged)
    Q_PROPERTY(bool presenceDimmingEnabled READ presenceDimmingEnabled WRITE setPresenceDimmingEnabled NOTIFY configChanged)
    Q_PROPERTY(bool presenceLockEnabled READ presenceLockEnabled WRITE setPresenceLockEnabled NOTIFY configChanged)
    Q_PROPERTY(bool waveGesturesEnabled READ waveGesturesEnabled WRITE setWaveGesturesEnabled NOTIFY configChanged)
    Q_PROPERTY(bool micHoldGestureEnabled READ micHoldGestureEnabled WRITE setMicHoldGestureEnabled NOTIFY configChanged)
    Q_PROPERTY(bool volumeGesturesEnabled READ volumeGesturesEnabled WRITE setVolumeGesturesEnabled NOTIFY configChanged)
    Q_PROPERTY(int dimTimeoutSeconds READ dimTimeoutSeconds WRITE setDimTimeoutSeconds NOTIFY configChanged)
    Q_PROPERTY(int lockTimeoutSeconds READ lockTimeoutSeconds WRITE setLockTimeoutSeconds NOTIFY configChanged)
    Q_PROPERTY(int dimBrightnessPercent READ dimBrightnessPercent WRITE setDimBrightnessPercent NOTIFY configChanged)

public:
    explicit SpatialPresencePlugin(QObject *parent, const QVariantList &args);
    ~SpatialPresencePlugin() override;

    void connected() override;
    QString dbusPath() const override;
    void receivePacket(const NetworkPacket &np) override;

public Q_SLOTS:
    Q_SCRIPTABLE bool isDimmed() const;
    Q_SCRIPTABLE bool presenceDimmingEnabled() const;
    Q_SCRIPTABLE void setPresenceDimmingEnabled(bool enabled);

    Q_SCRIPTABLE bool presenceLockEnabled() const;
    Q_SCRIPTABLE void setPresenceLockEnabled(bool enabled);

    Q_SCRIPTABLE bool waveGesturesEnabled() const;
    Q_SCRIPTABLE void setWaveGesturesEnabled(bool enabled);

    Q_SCRIPTABLE bool micHoldGestureEnabled() const;
    Q_SCRIPTABLE void setMicHoldGestureEnabled(bool enabled);

    Q_SCRIPTABLE bool volumeGesturesEnabled() const;
    Q_SCRIPTABLE void setVolumeGesturesEnabled(bool enabled);

    Q_SCRIPTABLE int dimTimeoutSeconds() const;
    Q_SCRIPTABLE void setDimTimeoutSeconds(int seconds);

    Q_SCRIPTABLE int lockTimeoutSeconds() const;
    Q_SCRIPTABLE void setLockTimeoutSeconds(int seconds);

    Q_SCRIPTABLE int dimBrightnessPercent() const;
    Q_SCRIPTABLE void setDimBrightnessPercent(int percent);

    Q_SCRIPTABLE void onGestureDetected(const QString &gestureType);
    Q_SCRIPTABLE void onPresenceChanged(const QString &state, int durationMs = 0);
    Q_SCRIPTABLE void restoreBrightness();
    Q_SCRIPTABLE void dimScreen();
    Q_SCRIPTABLE void lockSession();

Q_SIGNALS:
    Q_SCRIPTABLE void gestureTriggered(const QString &gesture);
    Q_SCRIPTABLE void presenceChanged(bool present);
    Q_SCRIPTABLE void dimStateChanged(bool dimmed);
    Q_SCRIPTABLE void configChanged();

private Q_SLOTS:
    void onDimTimeout();
    void onLockTimeout();
    void onFadeStep();
    void onPluginConfigChanged();

private:
    int getSystemBrightness() const;
    int getMaxBrightness() const;
    void setSystemBrightnessSilent(int value);
    void startFadeTo(int targetBrightness);

    bool m_isDimmed = false;
    int m_savedBrightness = -1;
    int m_fadeTarget = -1;
    int m_fadeStepCount = 0;
    int m_fadeTotalSteps = 8;
    int m_fadeStartBrightness = -1;

    QTimer m_dimTimer;
    QTimer m_lockTimer;
    QTimer m_fadeTimer;

    // Config options
    bool m_optPresenceDimming = true;
    bool m_optPresenceLock = true;
    bool m_optWaveGestures = true;
    bool m_optMicHoldGesture = true;
    bool m_optVolumeGestures = true;
    int m_dimTimeoutSeconds = 15;
    int m_lockTimeoutSeconds = 60;
    int m_dimBrightnessPercent = 20;
};
