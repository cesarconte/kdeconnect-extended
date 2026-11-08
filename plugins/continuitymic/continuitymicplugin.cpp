/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "continuitymicplugin.h"
#include <KPluginFactory>
#include <QDebug>
#include <core/device.h>
#include <core/networkpacket.h>

K_PLUGIN_CLASS_WITH_JSON(ContinuityMicPlugin, "kdeconnect_continuitymic.json")

ContinuityMicPlugin::ContinuityMicPlugin(QObject *parent, const QVariantList &args)
    : KdeConnectPlugin(parent, args)
    , m_engine(new ContinuityMicEngine(this))
{
    connect(m_engine, &ContinuityMicEngine::stateChanged, this, &ContinuityMicPlugin::streamingChanged);
}

ContinuityMicPlugin::~ContinuityMicPlugin()
{
    stop();
}

QString ContinuityMicPlugin::dbusPath() const
{
    return QStringLiteral("/modules/kdeconnect/devices/%1/continuitymic").arg(device()->id());
}

bool ContinuityMicPlugin::isStreaming() const
{
    return m_engine->isRunning();
}

bool ContinuityMicPlugin::start()
{
    if (m_engine->isRunning()) {
        return true;
    }

    const QString devName = device()->name();
    if (!m_engine->start(devName)) {
        return false;
    }

    NetworkPacket req(PACKET_TYPE_CONTINUITYMIC_REQUEST);
    req.set(QStringLiteral("rate"), 48000);
    req.set(QStringLiteral("channels"), 2);
    req.set(QStringLiteral("format"), QStringLiteral("s16le"));
    sendPacket(req);

    return true;
}

void ContinuityMicPlugin::stop()
{
    if (m_engine->isRunning()) {
        m_engine->stop();

        NetworkPacket req(PACKET_TYPE_CONTINUITYMIC_STOP);
        sendPacket(req);
    }
}

void ContinuityMicPlugin::receivePacket(const NetworkPacket &np)
{
    if (np.type() == PACKET_TYPE_CONTINUITYMIC_AUDIO) {
        if (!m_engine->isRunning()) {
            return;
        }

        if (np.hasPayload()) {
            // Si el audio viene en el stream de payload
            QSharedPointer<QIODevice> payload = np.payload();
            if (payload && payload->isOpen()) {
                const QByteArray chunk = payload->readAll();
                m_engine->pushAudio(chunk);
            }
        } else if (np.has(QStringLiteral("pcm"))) {
            // Si el audio viene serializado en base64 en el cuerpo del paquete
            const QByteArray encoded = np.get<QByteArray>(QStringLiteral("pcm"));
            const QByteArray pcm = QByteArray::fromBase64(encoded);
            m_engine->pushAudio(pcm);
        }
    }
}

#include "continuitymicplugin.moc"
