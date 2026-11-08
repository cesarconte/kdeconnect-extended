/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "handoffplugin.h"

#include <core/daemon.h>
#include <core/device.h>
#include <core/networkpacket.h>

#include <KIO/OpenUrlJob>
#include <KLocalizedString>
#include <KNotification>
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
        auto *job = new KIO::OpenUrlJob(targetUrl);
        job->start();
    }
    clearActivity();
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

    // Also send standard share packet for Android KDE Connect compatibility
    NetworkPacket shareNp(PACKET_TYPE_SHARE_REQUEST);
    shareNp.set(QStringLiteral("url"), url.toUtf8());
    sendPacket(shareNp);
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

void HandoffPlugin::setActivity(const QString &url, const QString &title, const QString &sourceApp)
{
    if (url.isEmpty()) {
        return;
    }

    m_currentUrl = url;
    const QUrl parsedUrl = QUrl::fromUserInput(url);
    m_currentTitle = !title.isEmpty() ? title : (parsedUrl.isValid() ? parsedUrl.host() : url);
    m_sourceApp = !sourceApp.isEmpty() ? sourceApp : i18n("Navegador web");
    m_activityTimestamp = QDateTime::currentMSecsSinceEpoch();

    // Iniciar timer de expiración (10 minutos = 600.000 ms)
    m_expiryTimer.stop();
    m_expiryTimer.setSingleShot(true);
    m_expiryTimer.setInterval(600000);
    disconnect(&m_expiryTimer, &QTimer::timeout, this, &HandoffPlugin::clearActivity);
    connect(&m_expiryTimer, &QTimer::timeout, this, &HandoffPlugin::clearActivity);
    m_expiryTimer.start();

    // Notificación nativa tipo macOS Handoff
    auto *notif = new KNotification(QStringLiteral("notification"), KNotification::CloseOnTimeout);
    notif->setIconName(QStringLiteral("internet-web-browser"));
    notif->setComponentName(QStringLiteral("kdeconnect"));
    notif->setTitle(i18n("Continuar en este equipo"));
    notif->setText(i18n("Desde %1: %2", device()->name(), m_currentTitle));

    auto *openAction = notif->addAction(i18n("Abrir"));
    connect(openAction, &KNotificationAction::activated, this, [this, notif] {
        openCurrentActivity();
        notif->close();
    });

    auto *dismissAction = notif->addAction(i18n("Descartar"));
    connect(dismissAction, &KNotificationAction::activated, this, [this, notif] {
        clearActivity();
        notif->close();
    });

    notif->sendEvent();

    Q_EMIT activityReceived(m_currentUrl, m_currentTitle, m_sourceApp);
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
        const QString title = np.get<QString>(QStringLiteral("title"));
        const QString sourceApp = np.get<QString>(QStringLiteral("sourceApp"), i18n("Navegador web"));
        setActivity(url, title, sourceApp);
        return;
    }

    if (np.type() == PACKET_TYPE_SHARE_REQUEST) {
        if (np.has(QStringLiteral("url"))) {
            const QUrl url = QUrl::fromEncoded(np.get<QByteArray>(QStringLiteral("url")));
            if (url.isValid() && (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https"))) {
                const QString title = np.get<QString>(QStringLiteral("title"), url.host());
                setActivity(url.toString(), title, i18n("Navegador web"));
            }
        } else if (np.has(QStringLiteral("text"))) {
            const QString text = np.get<QString>(QStringLiteral("text"));
            const QUrl url = QUrl::fromUserInput(text);
            if (url.isValid() && (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https"))) {
                setActivity(url.toString(), url.host(), i18n("Navegador web"));
            }
        }
    }
}

QString HandoffPlugin::dbusPath() const
{
    return QStringLiteral("/modules/kdeconnect/devices/%1/handoff").arg(device()->id());
}

#include "handoffplugin.moc"
#include "moc_handoffplugin.cpp"
