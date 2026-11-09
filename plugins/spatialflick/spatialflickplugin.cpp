/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "spatialflickplugin.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KPluginFactory>
#include <QDBusInterface>
#include <QDBusReply>
#include <QFileInfo>
#include <QProcess>

#include <core/daemon.h>
#include <core/device.h>

K_PLUGIN_CLASS_WITH_JSON(SpatialFlickPlugin, "kdeconnect_spatialflick.json")

SpatialFlickPlugin::SpatialFlickPlugin(QObject *parent, const QVariantList &args)
    : KdeConnectPlugin(parent, args)
{
    m_portalEnabled = config()->getBool(QStringLiteral("portalEnabled"), true);
    m_edgePositionStr = config()->getString(QStringLiteral("edgePosition"), QStringLiteral("right"));
    m_soundEnabled = config()->getBool(QStringLiteral("soundEnabled"), true);
}

SpatialFlickPlugin::~SpatialFlickPlugin()
{
    cleanupOverlay();
}

void SpatialFlickPlugin::connected()
{
    initOverlay();
}

QString SpatialFlickPlugin::dbusPath() const
{
    return QStringLiteral("/modules/kdeconnect/devices/%1/spatialflick").arg(device()->id());
}

void SpatialFlickPlugin::initOverlay()
{
    if (m_overlay || !m_portalEnabled || !device()) {
        return;
    }

    m_overlay = new EdgePortalOverlay();
    m_overlay->setDeviceName(device()->name());
    m_overlay->setDeviceIconName(device()->iconName());

    if (m_edgePositionStr == QLatin1String("left")) {
        m_overlay->setEdgePosition(EdgePortalOverlay::EdgeLeft);
    } else if (m_edgePositionStr == QLatin1String("top")) {
        m_overlay->setEdgePosition(EdgePortalOverlay::EdgeTop);
    } else if (m_edgePositionStr == QLatin1String("bottom")) {
        m_overlay->setEdgePosition(EdgePortalOverlay::EdgeBottom);
    } else {
        m_overlay->setEdgePosition(EdgePortalOverlay::EdgeRight);
    }

    connect(m_overlay, &EdgePortalOverlay::filesDropped, this, &SpatialFlickPlugin::onFilesDropped);
    m_overlay->show();
}

void SpatialFlickPlugin::cleanupOverlay()
{
    if (m_overlay) {
        m_overlay->hide();
        m_overlay->deleteLater();
        m_overlay = nullptr;
    }
}

bool SpatialFlickPlugin::portalEnabled() const
{
    return m_portalEnabled;
}

void SpatialFlickPlugin::setPortalEnabled(bool enabled)
{
    if (m_portalEnabled != enabled) {
        m_portalEnabled = enabled;
        config()->set(QStringLiteral("portalEnabled"), enabled);
        Q_EMIT configChanged();

        if (enabled) {
            initOverlay();
        } else {
            cleanupOverlay();
        }
    }
}

QString SpatialFlickPlugin::edgePosition() const
{
    return m_edgePositionStr;
}

void SpatialFlickPlugin::setEdgePosition(const QString &edge)
{
    if (m_edgePositionStr != edge) {
        m_edgePositionStr = edge;
        config()->set(QStringLiteral("edgePosition"), edge);
        Q_EMIT configChanged();

        if (m_overlay) {
            if (edge == QLatin1String("left")) {
                m_overlay->setEdgePosition(EdgePortalOverlay::EdgeLeft);
            } else if (edge == QLatin1String("top")) {
                m_overlay->setEdgePosition(EdgePortalOverlay::EdgeTop);
            } else if (edge == QLatin1String("bottom")) {
                m_overlay->setEdgePosition(EdgePortalOverlay::EdgeBottom);
            } else {
                m_overlay->setEdgePosition(EdgePortalOverlay::EdgeRight);
            }
        }
    }
}

bool SpatialFlickPlugin::soundEnabled() const
{
    return m_soundEnabled;
}

void SpatialFlickPlugin::setSoundEnabled(bool enabled)
{
    if (m_soundEnabled != enabled) {
        m_soundEnabled = enabled;
        config()->set(QStringLiteral("soundEnabled"), enabled);
        Q_EMIT configChanged();
    }
}

void SpatialFlickPlugin::onFilesDropped(const QList<QUrl> &urls)
{
    QStringList paths;
    paths.reserve(urls.size());
    for (const auto &u : urls) {
        paths.append(u.isLocalFile() ? u.toLocalFile() : u.toString());
    }

    triggerFlickSend(paths);
}

void SpatialFlickPlugin::triggerFlickSend(const QStringList &urls)
{
    if (urls.isEmpty() || !device()) {
        return;
    }

    // 1. Delegar envío directo a kdeconnect_share
    QDBusInterface shareIface(QStringLiteral("org.kde.kdeconnect"),
                              QStringLiteral("/modules/kdeconnect/devices/%1/share").arg(device()->id()),
                              QStringLiteral("org.kde.kdeconnect.device.share"),
                              QDBusConnection::sessionBus());
    if (shareIface.isValid()) {
        shareIface.call(QDBus::NoBlock, QStringLiteral("shareUrls"), urls);
    } else {
        auto *sharePlugin = device()->plugin(QStringLiteral("kdeconnect_share"));
        if (!sharePlugin) {
            sharePlugin = device()->plugin(QStringLiteral("share"));
        }
        if (sharePlugin) {
            QMetaObject::invokeMethod(sharePlugin, "shareUrls", Q_ARG(QStringList, urls));
        }
    }

    // 2. Notificar por paquete de red Spatial Flick
    NetworkPacket np(PACKET_TYPE_SPATIAL_FLICK);
    np.set(QStringLiteral("action"), QStringLiteral("flick_sent"));
    np.set(QStringLiteral("fileCount"), urls.size());
    np.set(QStringLiteral("firstFile"), QFileInfo(urls.first()).fileName());
    sendPacket(np);

    // 3. Sonido y notificación de escritorio
    playPortalSound();

    QDBusInterface notifIface(QStringLiteral("org.kde.kdeconnect"),
                              QStringLiteral("/modules/kdeconnect"),
                              QStringLiteral("org.kde.kdeconnect.daemon"),
                              QDBusConnection::sessionBus());
    if (notifIface.isValid()) {
        QString summary = (urls.size() == 1) ? i18n("Lanzando %1", QFileInfo(urls.first()).fileName()) : i18n("Lanzando %1 archivos", urls.size());
        QString body = i18n("Enviando vía portal direccional a %1", device()->name());
        notifIface.call(QDBus::NoBlock,
                        QStringLiteral("sendSimpleNotification"),
                        QStringLiteral("spatial_flick_sent"),
                        summary,
                        body,
                        QStringLiteral("network-workgroup"));
    }

    Q_EMIT flickSent(urls);
}

void SpatialFlickPlugin::handleFlickReceived(const QString &filePath)
{
    if (filePath.isEmpty()) {
        return;
    }

    const QFileInfo fi(filePath);

    // Animación visual inversa en el portal del borde
    if (m_overlay) {
        m_overlay->showReceiveAnimation(fi.fileName());
    }

    playPortalSound();
    handleContextualTarget(filePath);

    // Notificación en Plasma
    QDBusInterface notifIface(QStringLiteral("org.kde.kdeconnect"),
                              QStringLiteral("/modules/kdeconnect"),
                              QStringLiteral("org.kde.kdeconnect.daemon"),
                              QDBusConnection::sessionBus());
    if (notifIface.isValid()) {
        notifIface.call(QDBus::NoBlock,
                        QStringLiteral("sendSimpleNotification"),
                        QStringLiteral("spatial_flick_received"),
                        i18n("🚀 Recibido por Spatial Flick"),
                        i18n("%1 desde %2", fi.fileName(), device()->name()),
                        QStringLiteral("network-workgroup"));
    }

    Q_EMIT flickReceived(filePath);
}

void SpatialFlickPlugin::handleContextualTarget(const QString &filePath)
{
    // Consultar ventana activa en KWin
    QDBusInterface kwin(QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"), QDBusConnection::sessionBus());
    if (kwin.isValid()) {
        QDBusReply<QVariantMap> reply = kwin.call(QStringLiteral("queryWindowInfo"));
        if (reply.isValid()) {
            const QVariantMap info = reply.value();
            const QString resClass = info.value(QStringLiteral("resourceClass")).toString().toLower();
            const QString title = info.value(QStringLiteral("caption")).toString().toLower();

            // Si Dolphin está enfocado, seleccionar el archivo directamente
            if (resClass.contains(QLatin1String("dolphin")) || title.contains(QLatin1String("dolphin"))) {
                QProcess::startDetached(QStringLiteral("dolphin"), {QStringLiteral("--select"), filePath});
                return;
            }

            // Si hay un editor o visor de fotos abierto (Gwenview, GIMP, Krita) y es imagen
            const QString ext = QFileInfo(filePath).suffix().toLower();
            const bool isImage = (ext == QLatin1String("png") || ext == QLatin1String("jpg") || ext == QLatin1String("jpeg") || ext == QLatin1String("webp")
                                  || ext == QLatin1String("gif"));
            if (isImage
                && (resClass.contains(QLatin1String("gwenview")) || resClass.contains(QLatin1String("gimp")) || resClass.contains(QLatin1String("krita")))) {
                QProcess::startDetached(QStringLiteral("xdg-open"), {filePath});
                return;
            }
        }
    }

    // Por defecto, abrir o resaltar en gestor de archivos
    QProcess::startDetached(QStringLiteral("dolphin"), {QStringLiteral("--select"), filePath});
}

void SpatialFlickPlugin::playPortalSound()
{
    if (!m_soundEnabled) {
        return;
    }

    QProcess::startDetached(QStringLiteral("canberra-gtk-play"),
                            {QStringLiteral("-i"), QStringLiteral("desktop-render"), QStringLiteral("-d"), QStringLiteral("kdeconnect_spatialflick")});
}

void SpatialFlickPlugin::receivePacket(const NetworkPacket &np)
{
    if (np.type() != PACKET_TYPE_SPATIAL_FLICK) {
        return;
    }

    const QString action = np.get<QString>(QStringLiteral("action"));
    if (action == QLatin1String("flick_received") || action == QLatin1String("flick")) {
        const QString filePath = np.get<QString>(QStringLiteral("path"));
        handleFlickReceived(filePath);
    }
}

void SpatialFlickPlugin::onPluginConfigChanged()
{
    m_portalEnabled = config()->getBool(QStringLiteral("portalEnabled"), true);
    m_edgePositionStr = config()->getString(QStringLiteral("edgePosition"), QStringLiteral("right"));
    m_soundEnabled = config()->getBool(QStringLiteral("soundEnabled"), true);

    if (m_portalEnabled) {
        if (!m_overlay) {
            initOverlay();
        } else {
            setEdgePosition(m_edgePositionStr);
        }
    } else {
        cleanupOverlay();
    }
}

#include "spatialflickplugin.moc"
