/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "kdeconnectrunner.h"

#include <KLocalizedString>
#include <KPluginFactory>
#include <QClipboard>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMimeDatabase>
#include <QMimeType>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>

#include "dbusinterfaces/dbusinterfaces.h"

K_PLUGIN_CLASS_WITH_JSON(KdeConnectRunner, "krunner_kdeconnect.json")

KdeConnectRunner::KdeConnectRunner(QObject *parent, const KPluginMetaData &metaData)
    : KRunner::AbstractRunner(parent, metaData)
{
    m_deviceActions = {KRunner::Action(QStringLiteral("ring"), QStringLiteral("irc-voice"), i18n("Hacer sonar teléfono")),
                       KRunner::Action(QStringLiteral("browse"), QStringLiteral("document-open-folder"), i18n("Explorar archivos (SFTP)")),
                       KRunner::Action(QStringLiteral("flick"), QStringLiteral("arrow-right-double"), i18n("Lanzar archivo al móvil (Spatial Flick)")),
                       KRunner::Action(QStringLiteral("sms"), QStringLiteral("message-new"), i18n("Abrir Mensajes SMS")),
                       KRunner::Action(QStringLiteral("clipboard"), QStringLiteral("klipper"), i18n("Enviar Portapapeles")),
                       KRunner::Action(QStringLiteral("mic"), QStringLiteral("audio-input-microphone"), i18n("Iniciar Micrófono de Continuidad"))};

    m_contactActions = {KRunner::Action(QStringLiteral("sms_contact"), QStringLiteral("message-new"), i18n("Enviar SMS")),
                        KRunner::Action(QStringLiteral("call_contact"), QStringLiteral("call-start"), i18n("Llamar / Marcar teléfono")),
                        KRunner::Action(QStringLiteral("copy_contact"), QStringLiteral("edit-copy"), i18n("Copiar número"))};
}

void KdeConnectRunner::match(KRunner::RunnerContext &context)
{
    const QString term = context.query().trimmed();
    if (term.isEmpty()) {
        return;
    }

    bool isExplicitPrefix = false;
    bool isContactPrefix = false;
    QString searchTerm = term;

    const QStringList contactPrefixes = {QStringLiteral("sms:"), QStringLiteral("contacto:"), QStringLiteral("contact:"), QStringLiteral("msg:")};
    for (const QString &cp : contactPrefixes) {
        if (term.startsWith(cp, Qt::CaseInsensitive)) {
            isExplicitPrefix = true;
            isContactPrefix = true;
            searchTerm = term.mid(cp.length()).trimmed();
            break;
        }
    }

    if (!isContactPrefix) {
        const QStringList generalPrefixes = {QStringLiteral("phone:"),
                                             QStringLiteral("movil:"),
                                             QStringLiteral("móvil:"),
                                             QStringLiteral("tel:"),
                                             QStringLiteral("cel:"),
                                             QStringLiteral("celular:"),
                                             QStringLiteral("file:"),
                                             QStringLiteral("archivo:")};
        for (const QString &prefix : generalPrefixes) {
            if (term.startsWith(prefix, Qt::CaseInsensitive)) {
                isExplicitPrefix = true;
                searchTerm = term.mid(prefix.length()).trimmed();
                break;
            }
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

        // 1. Montaje de SFTP solo ante prefijo explícito de archivos y con enfriamiento (cooldown) para no saturar sshfs
        if (isExplicitPrefix && !isContactPrefix) {
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (now - m_lastMountAttempt > 30000) {
                m_lastMountAttempt = now;
                QDBusInterface sftpIface(QStringLiteral("org.kde.kdeconnect"),
                                         QStringLiteral("/modules/kdeconnect/devices/%1/sftp").arg(deviceId),
                                         QStringLiteral("org.kde.kdeconnect.device.sftp"),
                                         QDBusConnection::sessionBus());
                if (sftpIface.isValid()) {
                    const bool isMounted = sftpIface.property("isMounted").toBool();
                    if (!isMounted) {
                        sftpIface.call(QDBus::NoBlock, QStringLiteral("mount"));
                    }
                }
            }
        }

        const bool matchesDeviceName =
            devName.contains(term, Qt::CaseInsensitive) || (!searchTerm.isEmpty() && devName.contains(searchTerm, Qt::CaseInsensitive));

        if ((queryMatchesKeyword || matchesDeviceName) && !isContactPrefix) {
            KRunner::QueryMatch match(this);
            match.setId(deviceId);
            match.setData(deviceId);
            match.setText(devName);
            match.setSubtext(i18n("Dispositivo KDE Connect conectado"));
            match.setIconName(!devIcon.isEmpty() ? devIcon : QStringLiteral("kdeconnect"));
            match.setCategoryRelevance(KRunner::QueryMatch::CategoryRelevance::Highest);
            match.setRelevance(matchesDeviceName ? 1.0 : 0.85);
            match.setActions(m_deviceActions);

            context.addMatch(match);
        }

        // 2. Búsqueda de Contactos / SMS en el teléfono
        if (!searchTerm.isEmpty() && (isContactPrefix || isExplicitPrefix || searchTerm.length() >= 3)) {
            searchContacts(context, deviceId, devName, searchTerm);
        }

        // 3. Búsqueda cruzada de archivos en el teléfono (si no es prefijo exclusivo de contactos)
        if (!isContactPrefix && !searchTerm.isEmpty() && (isExplicitPrefix || searchTerm.length() >= 3)) {
            searchRemoteFiles(context, deviceId, devName, searchTerm);
        }
    }
}

void KdeConnectRunner::searchContacts(KRunner::RunnerContext &context, const QString &deviceId, const QString &devName, const QString &searchTerm)
{
    QString vcardsLocation = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/kpeoplevcard/kdeconnect-") + deviceId;
    QDir dir(vcardsLocation);
    if (!dir.exists()) {
        vcardsLocation = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + QStringLiteral("/Contacts/kdeconnect-") + deviceId;
        dir = QDir(vcardsLocation);
        if (!dir.exists()) {
            return;
        }
    }

    const QFileInfoList vcfFiles = dir.entryInfoList({QStringLiteral("*.vcf"), QStringLiteral("*.vcard")}, QDir::Files, QDir::Time);
    int matchesFound = 0;
    const int maxMatches = 10;

    for (const QFileInfo &vcfFile : vcfFiles) {
        if (!context.isValid()) {
            return;
        }

        QFile file(vcfFile.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        QString name;
        QString phone;

        QTextStream in(&file);
        while (!in.atEnd()) {
            const QString line = in.readLine();
            if (line.startsWith(QLatin1String("FN:"), Qt::CaseInsensitive)) {
                name = line.mid(3).trimmed();
            } else if (line.contains(QLatin1String("TEL"), Qt::CaseInsensitive) && line.contains(QLatin1Char(':'))) {
                phone = line.section(QLatin1Char(':'), 1).trimmed();
            }
        }
        file.close();

        if (name.isEmpty()) {
            continue;
        }

        const bool nameMatches = name.contains(searchTerm, Qt::CaseInsensitive);
        const bool phoneMatches = !phone.isEmpty() && phone.contains(searchTerm, Qt::CaseInsensitive);

        if (nameMatches || phoneMatches) {
            KRunner::QueryMatch match(this);
            match.setId(QStringLiteral("kdeconnect:contact:") + deviceId + QLatin1Char(':') + name);

            QVariantMap contactData;
            contactData[QStringLiteral("type")] = QStringLiteral("contact");
            contactData[QStringLiteral("deviceId")] = deviceId;
            contactData[QStringLiteral("name")] = name;
            contactData[QStringLiteral("phone")] = phone;
            match.setData(contactData);

            match.setText(name);
            const QString sub = phone.isEmpty() ? i18n("Contacto · %1", devName) : i18n("📱 %1 (Contacto · %2)", phone, devName);
            match.setSubtext(sub);
            match.setIconName(QStringLiteral("user-identity"));
            match.setCategoryRelevance(KRunner::QueryMatch::CategoryRelevance::Highest);
            match.setRelevance(nameMatches ? 0.95 : 0.85);
            match.setActions(m_contactActions);

            context.addMatch(match);

            if (++matchesFound >= maxMatches) {
                return;
            }
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

    // Identificar raíces de almacenamiento en el móvil (almacenamiento interno Android o tarjetas SD)
    QStringList storageRoots;
    const QString emulated = mountDir + QStringLiteral("/storage/emulated/0");
    if (QDir(emulated).exists()) {
        storageRoots << emulated;
    }
    const QDir storageDir(mountDir + QStringLiteral("/storage"));
    if (storageDir.exists()) {
        const QStringList entries = storageDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &entry : entries) {
            if (entry != QLatin1String("emulated") && entry != QLatin1String("self")) {
                const QString sdPath = storageDir.absoluteFilePath(entry);
                if (!storageRoots.contains(sdPath)) {
                    storageRoots << sdPath;
                }
            }
        }
    }
    if (storageRoots.isEmpty()) {
        storageRoots << mountDir;
    }

    // Subdirectorios principales donde los usuarios guardan contenido
    const QStringList subFolderCandidates = {QStringLiteral(""),
                                             QStringLiteral("Download"),
                                             QStringLiteral("DCIM"),
                                             QStringLiteral("DCIM/Camera"),
                                             QStringLiteral("Pictures"),
                                             QStringLiteral("Pictures/Screenshots"),
                                             QStringLiteral("Documents"),
                                             QStringLiteral("Music"),
                                             QStringLiteral("Movies"),
                                             QStringLiteral("MIUI")};

    QMimeDatabase mimeDb;
    int visited = 0;
    int matchesFound = 0;
    const int maxVisited = 350;
    const int maxMatches = 15;

    for (const QString &baseRoot : storageRoots) {
        for (const QString &relSubDir : subFolderCandidates) {
            if (!context.isValid()) {
                return;
            }

            const QString targetPath = relSubDir.isEmpty() ? baseRoot : (baseRoot + QLatin1Char('/') + relSubDir);
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
                    match.setRelevance(0.85);

                    context.addMatch(match);
                    if (++matchesFound >= maxMatches) {
                        return;
                    }
                }
            }
        }
    }
}

void KdeConnectRunner::run(const KRunner::RunnerContext &context, const KRunner::QueryMatch &match)
{
    Q_UNUSED(context)

    // 1. Si es un archivo del móvil emparejado, abrirlo con la aplicación predeterminada de Plasma
    if (match.data().typeId() == QMetaType::QString && match.data().toString().startsWith(QLatin1Char('/'))) {
        const QString filePath = match.data().toString();
        if (!filePath.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        }
        return;
    }

    // 2. Si es un contacto del teléfono
    if (match.data().typeId() == QMetaType::QVariantMap) {
        const QVariantMap contactData = match.data().toMap();
        const QString deviceId = contactData.value(QStringLiteral("deviceId")).toString();
        const QString phone = contactData.value(QStringLiteral("phone")).toString();
        const QString actionId = match.selectedAction().id();

        if (actionId == QLatin1String("call_contact")) {
            QString cleanPhone = phone;
            cleanPhone.remove(QLatin1Char(' ')).remove(QLatin1Char('-'));
            QDesktopServices::openUrl(QUrl(QStringLiteral("tel:") + cleanPhone));
        } else if (actionId == QLatin1String("copy_contact")) {
            if (QGuiApplication::clipboard()) {
                QGuiApplication::clipboard()->setText(phone);
            }
        } else {
            // Acción predeterminada o "sms_contact": abrir la aplicación de SMS con ese dispositivo
            QProcess::startDetached(QStringLiteral("kdeconnect-sms"), {QStringLiteral("--device"), deviceId});
        }
        return;
    }

    // 3. Si es el dispositivo propiamente dicho
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
    } else if (actionId == QLatin1String("sms")) {
        QProcess::startDetached(QStringLiteral("kdeconnect-sms"), {QStringLiteral("--device"), deviceId});
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
