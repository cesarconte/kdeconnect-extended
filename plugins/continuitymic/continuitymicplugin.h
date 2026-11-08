/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include "continuitymicengine.h"
#include <KPluginMetaData>
#include <core/kdeconnectplugin.h>

#define PACKET_TYPE_CONTINUITYMIC_REQUEST QStringLiteral("kdeconnect.continuitymic.request")
#define PACKET_TYPE_CONTINUITYMIC_AUDIO QStringLiteral("kdeconnect.continuitymic.audio")
#define PACKET_TYPE_CONTINUITYMIC_STOP QStringLiteral("kdeconnect.continuitymic.stop")

class ContinuityMicPlugin : public KdeConnectPlugin
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeconnect.device.continuitymic")
    Q_PROPERTY(bool isStreaming READ isStreaming NOTIFY streamingChanged)

public:
    explicit ContinuityMicPlugin(QObject *parent, const QVariantList &args);
    ~ContinuityMicPlugin() override;

    Q_SCRIPTABLE bool start();
    Q_SCRIPTABLE void stop();
    Q_SCRIPTABLE bool isStreaming() const;

    QString dbusPath() const override;
    void receivePacket(const NetworkPacket &np) override;

Q_SIGNALS:
    Q_SCRIPTABLE void streamingChanged(bool isStreaming);

private:
    ContinuityMicEngine *m_engine = nullptr;
};
