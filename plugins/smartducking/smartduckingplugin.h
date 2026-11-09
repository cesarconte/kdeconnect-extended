/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QElapsedTimer>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QTimer>

#include <core/kdeconnectplugin.h>

namespace PulseAudioQt
{
class Sink;
}

#define PACKET_TYPE_TELEPHONY QStringLiteral("kdeconnect.telephony")

class SmartDuckingPlugin : public KdeConnectPlugin
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeconnect.device.smartducking")
    Q_PROPERTY(bool isDucked READ isDucked NOTIFY duckedChanged)
    Q_PROPERTY(int duckingLevel READ duckingLevel WRITE setDuckingLevel NOTIFY duckingLevelChanged)
    Q_PROPERTY(bool voiceIsolationActive READ voiceIsolationActive NOTIFY voiceIsolationChanged)

public:
    explicit SmartDuckingPlugin(QObject *parent, const QVariantList &args);
    ~SmartDuckingPlugin() override;

    void receivePacket(const NetworkPacket &np) override;
    QString dbusPath() const override;

    bool isDucked() const;
    int duckingLevel() const;
    void setDuckingLevel(int level);
    bool voiceIsolationActive() const;

public Q_SLOTS:
    Q_SCRIPTABLE void triggerDucking(bool duck);
    Q_SCRIPTABLE void setDuckingPercentage(int percent);
    Q_SCRIPTABLE void toggleVoiceIsolation(bool enable);

Q_SIGNALS:
    Q_SCRIPTABLE void duckedChanged(bool ducked);
    Q_SCRIPTABLE void duckingLevelChanged(int level);
    Q_SCRIPTABLE void voiceIsolationChanged(bool active);

private Q_SLOTS:
    void onRampStep();
    void onRestoreStep();

private:
    void startDucking(int targetPercent, int durationMs);
    void startRestoring(int durationMs);
    void pauseMprisPlayers();
    void resumeMprisPlayers();
    void applyVoiceIsolationPreset();
    void restoreVoiceIsolationPreset();

    bool m_isDucked = false;
    int m_duckingLevel = 15;
    int m_rampDurationMs = 450;
    int m_restoreDurationMs = 550;

    QMap<QString, qint64> m_originalVolumes;
    QMap<QString, qint64> m_startRampVolumes;
    QMap<QString, qint64> m_targetRampVolumes;

    QTimer m_rampTimer;
    QTimer m_restoreTimer;
    QElapsedTimer m_animElapsed;
    int m_currentAnimDuration = 450;

    bool m_pauseMpris = true;
    QSet<QString> m_pausedMprisPlayers;

    bool m_enableVoiceIsolation = true;
    bool m_voiceIsolationActive = false;
    QString m_savedEasyEffectsPreset;
};
