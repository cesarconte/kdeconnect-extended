/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "continuitycameraplugin.h"
#include <KPluginFactory>
#include <QDebug>
#include <core/device.h>
#include <core/networkpacket.h>

K_PLUGIN_CLASS_WITH_JSON(ContinuityCameraPlugin, "kdeconnect_continuitycamera.json")

ContinuityCameraPlugin::ContinuityCameraPlugin(QObject *parent, const QVariantList &args)
    : KdeConnectPlugin(parent, args)
    , m_engine(new ContinuityCameraEngine(this))
{
    connect(m_engine, &ContinuityCameraEngine::stateChanged, this, &ContinuityCameraPlugin::streamingChanged);
}

ContinuityCameraPlugin::~ContinuityCameraPlugin()
{
    stop();
}

QString ContinuityCameraPlugin::dbusPath() const
{
    return QStringLiteral("/modules/kdeconnect/devices/%1/continuitycamera").arg(device()->id());
}

bool ContinuityCameraPlugin::isStreaming() const
{
    return m_engine->isRunning();
}

bool ContinuityCameraPlugin::start(int width, int height)
{
    if (m_engine->isRunning()) {
        return true;
    }

    const QString devName = device()->name();
    if (!m_engine->start(devName, static_cast<uint32_t>(width), static_cast<uint32_t>(height))) {
        return false;
    }

    NetworkPacket req(PACKET_TYPE_CONTINUITYCAMERA_REQUEST);
    req.set(QStringLiteral("width"), width);
    req.set(QStringLiteral("height"), height);
    req.set(QStringLiteral("fps"), 30);
    sendPacket(req);

    return true;
}

void ContinuityCameraPlugin::stop()
{
    if (m_engine->isRunning()) {
        m_engine->stop();

        NetworkPacket req(PACKET_TYPE_CONTINUITYCAMERA_STOP);
        sendPacket(req);
    }
}

void ContinuityCameraPlugin::receivePacket(const NetworkPacket &np)
{
    if (np.type() == PACKET_TYPE_CONTINUITYCAMERA_FRAME) {
        if (!m_engine->isRunning()) {
            return;
        }

        if (np.hasPayload()) {
            QSharedPointer<QIODevice> payload = np.payload();
            if (payload && payload->isOpen()) {
                const QByteArray chunk = payload->readAll();
                m_engine->pushJpegData(chunk);
            }
        } else if (np.has(QStringLiteral("data"))) {
            const QByteArray encoded = np.get<QByteArray>(QStringLiteral("data"));
            const QByteArray raw = QByteArray::fromBase64(encoded);
            m_engine->pushJpegData(raw);
        } else if (np.has(QStringLiteral("jpeg"))) {
            const QByteArray encoded = np.get<QByteArray>(QStringLiteral("jpeg"));
            const QByteArray raw = QByteArray::fromBase64(encoded);
            m_engine->pushJpegData(raw);
        }
    } else if (np.type() == PACKET_TYPE_CONTINUITYCAMERA_STOP) {
        if (m_engine->isRunning()) {
            m_engine->stop();
        }
    }
}

#include "continuitycameraplugin.moc"
