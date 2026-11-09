/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QPointer>
#include <QString>
#include <QUrl>

#include <core/kdeconnectplugin.h>

#include "edgeportaloverlay.h"

#define PACKET_TYPE_SPATIAL_FLICK QStringLiteral("kdeconnect.spatialflick")

class SpatialFlickPlugin : public KdeConnectPlugin
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeconnect.device.spatialflick")
    Q_PROPERTY(bool portalEnabled READ portalEnabled WRITE setPortalEnabled NOTIFY configChanged)
    Q_PROPERTY(QString edgePosition READ edgePosition WRITE setEdgePosition NOTIFY configChanged)
    Q_PROPERTY(bool soundEnabled READ soundEnabled WRITE setSoundEnabled NOTIFY configChanged)

public:
    explicit SpatialFlickPlugin(QObject *parent, const QVariantList &args);
    ~SpatialFlickPlugin() override;

    void connected() override;
    QString dbusPath() const override;
    void receivePacket(const NetworkPacket &np) override;

public Q_SLOTS:
    Q_SCRIPTABLE bool portalEnabled() const;
    Q_SCRIPTABLE void setPortalEnabled(bool enabled);

    Q_SCRIPTABLE QString edgePosition() const;
    Q_SCRIPTABLE void setEdgePosition(const QString &edge);

    Q_SCRIPTABLE bool soundEnabled() const;
    Q_SCRIPTABLE void setSoundEnabled(bool enabled);

    Q_SCRIPTABLE void triggerFlickSend(const QStringList &urls);
    Q_SCRIPTABLE void handleFlickReceived(const QString &filePath);

Q_SIGNALS:
    Q_SCRIPTABLE void flickSent(const QStringList &urls);
    Q_SCRIPTABLE void flickReceived(const QString &filePath);
    Q_SCRIPTABLE void configChanged();

private Q_SLOTS:
    void onFilesDropped(const QList<QUrl> &urls);
    void onPluginConfigChanged();

private:
    void initOverlay();
    void cleanupOverlay();
    void playPortalSound();
    void handleContextualTarget(const QString &filePath);

    QPointer<EdgePortalOverlay> m_overlay;

    bool m_portalEnabled = true;
    QString m_edgePositionStr = QStringLiteral("right");
    bool m_soundEnabled = true;
};
