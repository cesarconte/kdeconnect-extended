/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "kdeconnectrunner.h"

#include <KLocalizedString>
#include <KPluginFactory>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

#include "dbusinterfaces/dbusinterfaces.h"

K_PLUGIN_CLASS_WITH_JSON(KdeConnectRunner, "krunner_kdeconnect.json")

KdeConnectRunner::KdeConnectRunner(QObject *parent, const KPluginMetaData &metaData)
    : KRunner::AbstractRunner(parent, metaData)
{
    m_actions = {KRunner::Action(QStringLiteral("ring"), QStringLiteral("irc-voice"), i18n("Hacer sonar teléfono")),
                 KRunner::Action(QStringLiteral("browse"), QStringLiteral("document-open-folder"), i18n("Explorar archivos (SFTP)")),
                 KRunner::Action(QStringLiteral("flick"), QStringLiteral("arrow-right-double"), i18n("Lanzar archivo al móvil (Spatial Flick)")),
                 KRunner::Action(QStringLiteral("clipboard"), QStringLiteral("klipper"), i18n("Enviar Portapapeles")),
                 KRunner::Action(QStringLiteral("mic"), QStringLiteral("audio-input-microphone"), i18n("Iniciar Micrófono de Continuidad"))};
}

void KdeConnectRunner::match(KRunner::RunnerContext &context)
{
    const QString term = context.query().trimmed();
    if (term.isEmpty()) {
        return;
    }

    bool isExplicitPrefix = false;
    QString searchTerm = term;
    const QStringList prefixes = {QStringLiteral("phone:"),
                                  QStringLiteral("movil:"),
                                  QStringLiteral("móvil:"),
                                  QStringLiteral("tel:"),
                                  QStringLiteral("cel:"),
                                  QStringLiteral("celular:")};
    for (const QString &prefix : prefixes) {
        if (term.startsWith(prefix, Qt::CaseInsensitive)) {
            isExplicitPrefix = true;
            searchTerm = term.mid(prefix.length()).trimmed();
            break;
        }
    }

    const bool queryMatchesKeyword =
        term.startsWith(QStringLiteral("kde"), Qt::CaseInsensitive) || term.startsWith(QStringLiteral("connect"), Qt::CaseInsensitive) || isExplicitPrefix;

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

        const bool matchesDeviceName =
            devName.contains(term, Qt::CaseInsensitive) || (!searchTerm.isEmpty() && devName.contains(searchTerm, Qt::CaseInsensitive));

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

        // Búsqueda cruzada de archivos en el móvil (Cross-Device Search)
        if (!searchTerm.isEmpty() && (isExplicitPrefix || searchTerm.length() >= 3)) {
            searchRemoteFiles(context, deviceId, devName, searchTerm);
        }
    }
}

void KdeConnectRunner::searchRemoteFiles(KRunner::RunnerContext &context, const QString &deviceId, const QString &devName, const QString &searchTerm)
{
    QString runtimePath = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtimePath.isEmpty()) {
        runtimePath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    const QString mountDir = QDir(runtimePath).absoluteFilePath(deviceId);
    QDir dir(mountDir);
    if (!dir.exists()) {
        return;
    }

    // Directorios prioritarios en el almacenamiento del dispositivo móvil
    const QStringList searchDirs = {QStringLiteral(""),
                                    QStringLiteral("DCIM"),
                                    QStringLiteral("DCIM/Camera"),
                                    QStringLiteral("Pictures"),
                                    QStringLiteral("Pictures/Screenshots"),
                                    QStringLiteral("Download"),
                                    QStringLiteral("Documents"),
                                    QStringLiteral("Music"),
                                    QStringLiteral("Movies")};

    QMimeDatabase mimeDb;
    int visited = 0;
    int matchesFound = 0;
    const int maxVisited = 300;
    const int maxMatches = 12;

    for (const QString &relSubDir : searchDirs) {
        if (!context.isValid()) {
            return;
        }

        const QString targetPath = relSubDir.isEmpty() ? mountDir : (mountDir + QLatin1Char('/') + relSubDir);
        QDir subDir(targetPath);
        if (!subDir.exists()) {
            continue;
        }

        const QFileInfoList entries = subDir.entryInfoList(QDir::Files | QDir::Readable | QDir::NoDotAndDotDot, QDir::Time);
        for (const QFileInfo &entry : entries) {
            if (!context.isValid()) {
                return;
            }

            if (++visited > maxVisited) {
                return;
            }

            const QString fileName = entry.fileName();
            if (fileName.startsWith(QLatin1Char('.'))) {
                continue;
            }

            if (fileName.contains(searchTerm, Qt::CaseInsensitive)) {
                KRunner::QueryMatch match(this);
                match.setId(QStringLiteral("kdeconnect:file:") + entry.absoluteFilePath());
                match.setData(entry.absoluteFilePath());
                match.setText(fileName);

                const QString parentFolder = entry.dir().dirName();
                match.setSubtext(i18n("En %1 · %2", devName, parentFolder.isEmpty() ? i18n("Almacenamiento") : parentFolder));

                const QMimeType mime = mimeDb.mimeTypeForFile(entry.absoluteFilePath(), QMimeDatabase::MatchExtension);
                match.setIconName(!mime.iconName().isEmpty() ? mime.iconName() : QStringLiteral("document-open"));
                match.setCategoryRelevance(KRunner::QueryMatch::CategoryRelevance::High);
                match.setRelevance(0.8);

                context.addMatch(match);
                if (++matchesFound >= maxMatches) {
                    return;
                }
            }
        }
    }
}

void KdeConnectRunner::run(const KRunner::RunnerContext &context, const KRunner::QueryMatch &match)
{
    Q_UNUSED(context)

    // Si es un archivo del móvil emparejado, abrirlo con la aplicación predeterminada de Plasma
    if (match.id().startsWith(QLatin1String("kdeconnect:file:")) || match.data().toString().startsWith(QLatin1Char('/'))) {
        const QString filePath = match.data().toString();
        if (!filePath.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        }
        return;
    }

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
    } else if (actionId == QLatin1String("flick")) {
        QProcess::startDetached(QStringLiteral("kdeconnect-handler"), {QStringLiteral("--open")});
    } else if (actionId == QLatin1String("clipboard")) {
        deviceIface.pluginCall(QStringLiteral("clipboard"), QStringLiteral("sendClipboard"));
    } else if (actionId == QLatin1String("mic")) {
        deviceIface.pluginCall(QStringLiteral("continuitymic"), QStringLiteral("start"));
    } else {
        // Acción por defecto: abrir la aplicación de KDE Connect
        QProcess::startDetached(QStringLiteral("kdeconnect-app"), {QStringLiteral("--device"), deviceId});
    }
}

#include "kdeconnectrunner.moc"
