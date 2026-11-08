/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include "continuitycameraengine.h"
#include <KPluginMetaData>
#include <core/kdeconnectplugin.h>

#define PACKET_TYPE_CONTINUITYCAMERA_REQUEST QStringLiteral("kdeconnect.continuitycamera.request")
#define PACKET_TYPE_CONTINUITYCAMERA_STOP QStringLiteral("kdeconnect.continuitycamera.stop")
#define PACKET_TYPE_CONTINUITYCAMERA_FRAME QStringLiteral("kdeconnect.continuitycamera.frame")
#define PACKET_TYPE_CONTINUITYCAMERA_STATE QStringLiteral("kdeconnect.continuitycamera.state")

class ContinuityCameraPlugin : public KdeConnectPlugin
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeconnect.device.continuitycamera")
    Q_PROPERTY(bool isStreaming READ isStreaming NOTIFY streamingChanged)

public:
    explicit ContinuityCameraPlugin(QObject *parent, const QVariantList &args);
    ~ContinuityCameraPlugin() override;

    Q_SCRIPTABLE bool start(int width = 1280, int height = 720);
    Q_SCRIPTABLE void stop();
    Q_SCRIPTABLE bool isStreaming() const;

    QString dbusPath() const override;
    void receivePacket(const NetworkPacket &np) override;

Q_SIGNALS:
    Q_SCRIPTABLE void streamingChanged(bool isStreaming);

private:
    ContinuityCameraEngine *m_engine = nullptr;
};
