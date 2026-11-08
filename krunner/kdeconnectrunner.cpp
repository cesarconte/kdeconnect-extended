/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "kdeconnectrunner.h"

#include <KLocalizedString>
#include <KPluginFactory>
#include <QDBusInterface>
#include <QDBusReply>
#include <QProcess>

#include "dbusinterfaces/dbusinterfaces.h"

K_PLUGIN_CLASS_WITH_JSON(KdeConnectRunner, "krunner_kdeconnect.json")

KdeConnectRunner::KdeConnectRunner(QObject *parent, const KPluginMetaData &metaData)
    : KRunner::AbstractRunner(parent, metaData)
{
    m_actions = {KRunner::Action(QStringLiteral("ring"), QStringLiteral("irc-voice"), i18n("Hacer sonar teléfono")),
                 KRunner::Action(QStringLiteral("browse"), QStringLiteral("document-open-folder"), i18n("Explorar archivos (SFTP)")),
                 KRunner::Action(QStringLiteral("clipboard"), QStringLiteral("klipper"), i18n("Enviar Portapapeles")),
                 KRunner::Action(QStringLiteral("mic"), QStringLiteral("audio-input-microphone"), i18n("Iniciar Micrófono de Continuidad"))};
}

void KdeConnectRunner::match(KRunner::RunnerContext &context)
{
    const QString term = context.query().trimmed();
    if (term.isEmpty()) {
        return;
    }

    const bool queryMatchesKeyword =
        term.startsWith(QStringLiteral("kde"), Qt::CaseInsensitive) || term.startsWith(QStringLiteral("connect"), Qt::CaseInsensitive);

    DaemonDbusInterface daemonIface;
    if (!daemonIface.isValid()) {
        return;
    }

    const QStringList deviceIds = daemonIface.devices(true, true);
    for (const QString &deviceId : deviceIds) {
        if (!context.isValid()) {
            return;
        }

        DeviceDbusInterface deviceIface(deviceId);
        if (!deviceIface.isValid()) {
            continue;
        }

        const QString devName = deviceIface.name();
        const QString devIcon = deviceIface.iconName();

        const bool matchesDeviceName = devName.contains(term, Qt::CaseInsensitive);

        if (queryMatchesKeyword || matchesDeviceName) {
            KRunner::QueryMatch match(this);
            match.setId(deviceId);
            match.setData(deviceId);
            match.setText(devName);
            match.setSubtext(i18n("Dispositivo KDE Connect conectado"));
            match.setIconName(!devIcon.isEmpty() ? devIcon : QStringLiteral("kdeconnect"));
            match.setCategoryRelevance(KRunner::QueryMatch::CategoryRelevance::Highest);
            match.setRelevance(matchesDeviceName ? 1.0 : 0.85);
            match.setActions(m_actions);

            context.addMatch(match);
        }
    }
}

void KdeConnectRunner::run(const KRunner::RunnerContext &context, const KRunner::QueryMatch &match)
{
    Q_UNUSED(context)
    const QString deviceId = match.data().toString().isEmpty() ? match.id() : match.data().toString();
    const QString actionId = match.selectedAction().id();

    DeviceDbusInterface deviceIface(deviceId);
    if (!deviceIface.isValid()) {
        return;
    }

    if (actionId == QLatin1String("ring")) {
        deviceIface.pluginCall(QStringLiteral("findmyphone"), QStringLiteral("ring"));
    } else if (actionId == QLatin1String("browse")) {
        deviceIface.pluginCall(QStringLiteral("sftp"), QStringLiteral("startBrowsing"));
    } else if (actionId == QLatin1String("clipboard")) {
        deviceIface.pluginCall(QStringLiteral("clipboard"), QStringLiteral("sendClipboard"));
    } else if (actionId == QLatin1String("mic")) {
        deviceIface.pluginCall(QStringLiteral("continuitymic"), QStringLiteral("start"));
    } else {
        // Acción por defecto: abrir la configuración de KDE Connect
        QProcess::startDetached(QStringLiteral("kdeconnect-app"), {QStringLiteral("--device"), deviceId});
    }
}

#include "kdeconnectrunner.moc"
