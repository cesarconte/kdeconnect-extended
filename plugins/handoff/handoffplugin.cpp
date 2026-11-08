/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "handoffplugin.h"

#include <core/daemon.h>
#include <core/device.h>
#include <core/networkpacket.h>

#include <KLocalizedString>
#include <KPluginFactory>
#include <QDesktopServices>
#include <QUrl>

K_PLUGIN_CLASS_WITH_JSON(HandoffPlugin, "kdeconnect_handoff.json")

bool HandoffPlugin::hasActiveActivity() const
{
    return !m_currentUrl.isEmpty();
}

QString HandoffPlugin::currentUrl() const
{
    return m_currentUrl;
}

QString HandoffPlugin::currentTitle() const
{
    return m_currentTitle;
}

QString HandoffPlugin::sourceApp() const
{
    return m_sourceApp;
}

qint64 HandoffPlugin::activityTimestamp() const
{
    return m_activityTimestamp;
}

void HandoffPlugin::openCurrentActivity()
{
    if (m_currentUrl.isEmpty()) {
        return;
    }

    const QUrl targetUrl = QUrl::fromUserInput(m_currentUrl);
    if (targetUrl.isValid()) {
        QDesktopServices::openUrl(targetUrl);
    }
}

void HandoffPlugin::broadcastActivity(const QString &url, const QString &title, const QString &sourceApp)
{
    if (url.isEmpty()) {
        return;
    }

    NetworkPacket np(PACKET_TYPE_HANDOFF_ACTIVITY);
    np.set(QStringLiteral("url"), url);
    np.set(QStringLiteral("title"), title);
    np.set(QStringLiteral("sourceApp"), sourceApp);
    np.set<qint64>(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());

    sendPacket(np);
}

void HandoffPlugin::clearActivity()
{
    if (m_currentUrl.isEmpty()) {
        return;
    }

    m_currentUrl.clear();
    m_currentTitle.clear();
    m_sourceApp.clear();
    m_activityTimestamp = 0;
    m_expiryTimer.stop();

    Q_EMIT activityChanged();
}

void HandoffPlugin::receivePacket(const NetworkPacket &np)
{
    if (np.type() == PACKET_TYPE_HANDOFF_CLEAR) {
        clearActivity();
        return;
    }

    if (np.type() == PACKET_TYPE_HANDOFF_ACTIVITY) {
        const QString url = np.get<QString>(QStringLiteral("url"));
        if (url.isEmpty()) {
            return;
        }

        m_currentUrl = url;
        m_currentTitle = np.get<QString>(QStringLiteral("title"), url);
        m_sourceApp = np.get<QString>(QStringLiteral("sourceApp"), QStringLiteral("Navegador"));
        m_activityTimestamp = np.get<qint64>(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());

        // Iniciar timer de expiración (10 minutos = 600.000 ms)
        m_expiryTimer.stop();
        m_expiryTimer.setSingleShot(true);
        m_expiryTimer.setInterval(600000);
        disconnect(&m_expiryTimer, &QTimer::timeout, this, &HandoffPlugin::clearActivity);
        connect(&m_expiryTimer, &QTimer::timeout, this, &HandoffPlugin::clearActivity);
        m_expiryTimer.start();

        Q_EMIT activityReceived(m_currentUrl, m_currentTitle, m_sourceApp);
        Q_EMIT activityChanged();
    }
}

QString HandoffPlugin::dbusPath() const
{
    return QStringLiteral("/modules/kdeconnect/devices/%1/handoff").arg(device()->id());
}

#include "handoffplugin.moc"
#include "moc_handoffplugin.cpp"
