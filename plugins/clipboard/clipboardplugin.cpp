/**
 * SPDX-FileCopyrightText: 2013 Albert Vaca <albertvaka@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "clipboardplugin.h"

#include "clipboardlistener.h"
#include "core/daemon.h"
#include "core/filetransferjob.h"
#include "plugin_clipboard_debug.h"

#include <KLocalizedString>
#include <KNotification>
#include <KPluginFactory>
#include <QBuffer>
#include <QEventLoop>
#include <QFileInfo>
#include <QImage>
#include <QMimeDatabase>
#include <QMimeType>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVariant>
#include <qloggingcategory.h>
#include <qstringliteral.h>
#include <qtypes.h>

static constexpr qsizetype ONE_MB = 1 * 1024u * 1024u;

K_PLUGIN_CLASS_WITH_JSON(ClipboardPlugin, "kdeconnect_clipboard.json")

ClipboardPlugin::ClipboardPlugin(QObject *parent, const QVariantList &args)
    : KdeConnectPlugin(parent, args)
    , autoShare(true)
    , sharePasswords(true)
    , cleanUrls(true)
    , ocrEnabled(true)
    , autoCopyOcrText(false)
{
    connect(ClipboardListener::instance(), &ClipboardListener::clipboardChanged, this, &ClipboardPlugin::clipboardChanged);
    connect(config(), &KdeConnectPluginConfig::configChanged, this, &ClipboardPlugin::configChanged);
    configChanged();
}

void ClipboardPlugin::connected()
{
    sendConnectPacket();
}

QString ClipboardPlugin::dbusPath() const
{
    return QLatin1String("/modules/kdeconnect/devices/%1/clipboard").arg(device()->id());
}

void ClipboardPlugin::clipboardChanged(const QVariant &content, ClipboardListener::ClipboardContentType contentType)
{
    if (!autoShare) {
        return;
    }

    if (contentType == ClipboardListener::ClipboardContentTypePassword) {
        if (!sharePasswords) {
            return;
        }
    }

    sendClipboard(content);
}

void ClipboardPlugin::configChanged()
{
    autoShare = config()->getBool(QStringLiteral("autoShare"), config()->getBool(QStringLiteral("sendUnknown"), true));
    sharePasswords = config()->getBool(QStringLiteral("sendPassword"), true);
    cleanUrls = config()->getBool(QStringLiteral("cleanUrls"), true);
    ocrEnabled = config()->getBool(QStringLiteral("ocrEnabled"), true);
    autoCopyOcrText = config()->getBool(QStringLiteral("autoCopyOcrText"), false);
    Q_EMIT autoShareDisabledChanged(isAutoShareDisabled());
}

bool ClipboardPlugin::isAutoShareDisabled()
{
    // Return true also if autoShare is enabled but disabled for passwords
    return !autoShare || !sharePasswords;
}

void ClipboardPlugin::sendClipboard()
{
    QVariant content = ClipboardListener::instance()->currentContent();
    sendClipboard(content);
}

void ClipboardPlugin::sendClipboard(const QVariant &content)
{
    const bool isFile = content.typeId() == QMetaType::QUrl || content.typeId() == QMetaType::QImage;
    const QSet<QString> remoteCapabilities = device()->incomingCapabilities();
    if (isFile && !remoteCapabilities.isEmpty() && !remoteCapabilities.contains(PACKET_TYPE_CLIPBOARD_FILE)) {
        qCDebug(KDECONNECT_PLUGIN_CLIPBOARD) << "Remote device doesn't support clipboard files, not syncing";
        return;
    }

    if (content.typeId() == QMetaType::QUrl) {
        NetworkPacket np{PACKET_TYPE_CLIPBOARD_FILE};

        QUrl url = content.toUrl();
        QSharedPointer<QFile> ioFile(new QFile(url.toLocalFile()));

        const qsizetype fileSize = ioFile->size();
        const qsizetype configSize = config()->getInt(QStringLiteral("maxClipboardFileSizeMB"), 50) * ONE_MB;

        qCDebug(KDECONNECT_PLUGIN_CLIPBOARD) << fileSize << " " << configSize;

        if (!QFileInfo(*ioFile).isFile()) {
            qCDebug(KDECONNECT_PLUGIN_CLIPBOARD) << "Could not open file " << url.toDisplayString();
            return;
        } else if (fileSize > configSize) {
            qCDebug(KDECONNECT_PLUGIN_CLIPBOARD) << "File exceeds the max clipboard file size " << url.toDisplayString();
            return;
        } else {
            np.setPayload(ioFile, ioFile->size());
            np.set<QString>(QStringLiteral("filename"), url.fileName());
        }

        sendPacket(np);
    } else if (content.typeId() == QMetaType::QImage) {
        const QImage imageData = qvariant_cast<QImage>(content);
        QSharedPointer<QBuffer> buffer(new QBuffer());
        buffer->open(QIODevice::ReadWrite);

        if (imageData.save(buffer.get(), CLIPBOARD_IMAGE_FORMAT, 100)) {
            const qsizetype imageSize = buffer->size();
            const qsizetype configSize = config()->getInt(QStringLiteral("maxClipboardFileSizeMB"), 50) * ONE_MB;

            if (imageSize > configSize) {
                qCDebug(KDECONNECT_PLUGIN_CLIPBOARD) << "Image exceeds the max clipboard file size";
                return;
            }

            NetworkPacket np{PACKET_TYPE_CLIPBOARD_FILE};

            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            const qint64 ttl = config()->getInt(QStringLiteral("clipboardTtlSeconds"), 120) * 1000;
            np.set<qint64>(QStringLiteral("timestamp"), now);
            np.set<qint64>(QStringLiteral("expiryTimestamp"), now + ttl);

            np.setPayload(buffer, buffer->size());
            np.set<QString>(QStringLiteral("filename"), QString::number(now) + CLIPBOARD_IMAGE_EXTENSION);

            sendPacket(np);
        } else {
            qCDebug(KDECONNECT_PLUGIN_CLIPBOARD) << "Could not copy clipboard image";
        }
    } else {
        NetworkPacket np{PACKET_TYPE_CLIPBOARD};

        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const qint64 ttl = config()->getInt(QStringLiteral("clipboardTtlSeconds"), 120) * 1000;
        np.set<qint64>(QStringLiteral("timestamp"), now);
        np.set<qint64>(QStringLiteral("expiryTimestamp"), now + ttl);

        QString contentStr = content.toString();
        if (cleanUrls) {
            bool wasSanitized = false;
            int strippedCount = 0;
            contentStr = sanitizeTracking(contentStr, wasSanitized, strippedCount);
            if (wasSanitized && strippedCount > 0) {
                static QString s_lastCleanedUrl;
                static qint64 s_lastCleanedTimestamp = 0;

                if (contentStr != s_lastCleanedUrl || (now - s_lastCleanedTimestamp) > 1500) {
                    s_lastCleanedUrl = contentStr;
                    s_lastCleanedTimestamp = now;

                    qCDebug(KDECONNECT_PLUGIN_CLIPBOARD) << "Cleaned" << strippedCount << "trackers from local clipboard link";
                    ClipboardListener::instance()->setText(contentStr);

                    Q_EMIT urlSanitized(contentStr, strippedCount);

                    auto *notif = new KNotification(QStringLiteral("notification"), KNotification::CloseOnTimeout);
                    notif->setComponentName(QStringLiteral("kdeconnect"));
                    notif->setTitle(i18n("🔗 Enlace Limpiado"));
                    notif->setText(i18n("Se han eliminado %1 parámetros de rastreo.", strippedCount));
                    notif->sendEvent();
                }
            }
        }

        np.set(QStringLiteral("content"), contentStr);
        sendPacket(np);
    }
}

void ClipboardPlugin::sendConnectPacket()
{
    if (!autoShare || (!sharePasswords && ClipboardListener::instance()->currentContentType() == ClipboardListener::ClipboardContentTypePassword)) {
        // avoid sharing clipboard if the content is no need to share
        return;
    }

    NetworkPacket np(PACKET_TYPE_CLIPBOARD_CONNECT,
                     {{QStringLiteral("content"), ClipboardListener::instance()->currentContent()},
                      {QStringLiteral("timestamp"), ClipboardListener::instance()->updateTimestamp()}});
    sendPacket(np);
}

void ClipboardPlugin::receivePacket(const NetworkPacket &np)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    // Universal Clipboard TTL Expiration Check (macOS Continuity style)
    if (np.has(QStringLiteral("expiryTimestamp"))) {
        const qint64 expiry = np.get<qint64>(QStringLiteral("expiryTimestamp"));
        if (now > expiry) {
            qCDebug(KDECONNECT_PLUGIN_CLIPBOARD) << "Discarding expired clipboard packet (TTL exceeded by" << (now - expiry) << "ms)";
            return;
        }
    } else if (np.has(QStringLiteral("timestamp"))) {
        const qint64 packetTime = np.get<qint64>(QStringLiteral("timestamp"));
        if (packetTime > 0 && (now - packetTime > DEFAULT_CLIPBOARD_TTL_MS)) {
            qCDebug(KDECONNECT_PLUGIN_CLIPBOARD) << "Discarding legacy clipboard packet exceeding default 2m TTL";
            return;
        }
    }

    if (np.type() == PACKET_TYPE_CLIPBOARD_FILE) {
        if (np.hasPayload() && np.has(QStringLiteral("filename"))) {
            QTemporaryDir tmpDir;
            tmpDir.setAutoRemove(false);

            const auto filename = np.get<QString>(QStringLiteral("filename"));
            const QString filePath = QDir::cleanPath(tmpDir.filePath(filename));
            if (!filePath.startsWith(tmpDir.path() + QStringLiteral("/"))) {
                qCWarning(KDECONNECT_PLUGIN_CLIPBOARD) << "Directory traversal detected";
                return;
            }
            const QUrl destination = QUrl::fromLocalFile(filePath);

            FileTransferJob *job = np.createPayloadTransferJob(destination);
            job->setProperty("destUrl", destination.toString());
            job->setProperty("immediateProgressReporting", true);

            connect(job, &KJob::result, this, [this, filePath](KJob *job) -> void {
                ClipboardListener::instance()->setFile(job);
                if (ocrEnabled) {
                    performOcrOnClipboardImage(filePath);
                }
            });

            job->start();
        } else {
            qCWarning(KDECONNECT_PLUGIN_CLIPBOARD) << "Ignoring file without payload or filename";
        }

        return;
    }

    QString content = np.get<QString>(QStringLiteral("content"));
    if (np.type() == PACKET_TYPE_CLIPBOARD) {
        if (cleanUrls) {
            bool wasSanitized = false;
            int strippedCount = 0;
            content = sanitizeTracking(content, wasSanitized, strippedCount);
            if (wasSanitized && strippedCount > 0) {
                qCDebug(KDECONNECT_PLUGIN_CLIPBOARD) << "Cleaned" << strippedCount << "trackers from incoming clipboard link";
                Q_EMIT urlSanitized(content, strippedCount);

                auto *notif = new KNotification(QStringLiteral("notification"), KNotification::CloseOnTimeout);
                notif->setComponentName(QStringLiteral("kdeconnect"));
                notif->setTitle(i18n("Clean Clipboard"));
                notif->setText(i18np("Removed %1 tracking parameter from link", "Removed %1 tracking parameters from link", strippedCount));
                notif->setIconName(QStringLiteral("edit-clear"));
                notif->sendEvent();
            }
        }

        ClipboardListener::instance()->setText(content);

        // Si está configurada la auto-expiración local para datos confidenciales recibidos
        const bool autoExpire = config()->getBool(QStringLiteral("autoExpireReceivedClipboard"), false);
        if (autoExpire) {
            const qint64 ttl = config()->getInt(QStringLiteral("clipboardTtlSeconds"), 120) * 1000;
            QPointer<ClipboardPlugin> guard(this);
            QTimer::singleShot(ttl, this, [content, guard]() {
                if (guard && ClipboardListener::instance()->currentContent().toString() == content) {
                    qCDebug(KDECONNECT_PLUGIN_CLIPBOARD) << "Purging expired Universal Clipboard content from system clipboard";
                    ClipboardListener::instance()->setText(QString());
                }
            });
        }
    } else if (np.type() == PACKET_TYPE_CLIPBOARD_CONNECT) {
        qint64 packetTime = np.get<qint64>(QStringLiteral("timestamp"));
        // If the packetTime is 0, it means the timestamp is unknown (so do nothing).
        if (packetTime == 0 || packetTime < ClipboardListener::instance()->updateTimestamp()) {
            qCWarning(KDECONNECT_PLUGIN_CLIPBOARD) << "Ignoring clipboard without timestamp";
            return;
        }

        if (np.has(QStringLiteral("content"))) {
            if (cleanUrls) {
                bool wasSanitized = false;
                int strippedCount = 0;
                content = sanitizeTracking(content, wasSanitized, strippedCount);
            }
            ClipboardListener::instance()->setText(content);
        }
    }
}

QString ClipboardPlugin::sanitizeUrl(const QString &url)
{
    bool wasSanitized = false;
    int strippedCount = 0;
    return sanitizeTracking(url, wasSanitized, strippedCount);
}

QString ClipboardPlugin::sanitizeText(const QString &text)
{
    bool wasSanitized = false;
    int strippedCount = 0;
    return sanitizeTracking(text, wasSanitized, strippedCount);
}

void ClipboardPlugin::triggerOcr(const QString &imagePath)
{
    performOcrOnClipboardImage(imagePath);
}

void ClipboardPlugin::stripFormatting()
{
    const QString plainText = ClipboardListener::instance()->currentContent().toString();
    if (plainText.isEmpty()) {
        return;
    }

    ClipboardListener::instance()->setText(plainText);

    auto *notif = new KNotification(QStringLiteral("notification"), KNotification::CloseOnTimeout);
    notif->setComponentName(QStringLiteral("kdeconnect"));
    notif->setTitle(i18n("✨ Pegado Limpio"));
    notif->setText(i18n("Formato enriquecido eliminado. Listo para pegar en texto plano."));
    notif->sendEvent();
}

void ClipboardPlugin::cleanCurrentClipboard()
{
    const QString current = ClipboardListener::instance()->currentContent().toString();
    if (current.isEmpty()) {
        return;
    }

    bool wasSanitized = false;
    int strippedCount = 0;
    const QString clean = sanitizeTracking(current, wasSanitized, strippedCount);
    if (wasSanitized && strippedCount > 0) {
        ClipboardListener::instance()->setText(clean);
        Q_EMIT urlSanitized(clean, strippedCount);

        auto *notif = new KNotification(QStringLiteral("notification"), KNotification::CloseOnTimeout);
        notif->setComponentName(QStringLiteral("kdeconnect"));
        notif->setTitle(i18n("🔗 Enlace Limpiado"));
        notif->setText(i18n("Se han eliminado %1 parámetros de rastreo.", strippedCount));
        notif->sendEvent();
    }
}

QUrl ClipboardPlugin::cleanSingleUrl(const QUrl &url, int &removedCount)
{
    if (!url.isValid() || (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))) {
        return url;
    }

    QUrlQuery query(url.query());
    if (query.isEmpty()) {
        return url;
    }

    static const QSet<QString> exactTrackers = {
        QStringLiteral("si"),         QStringLiteral("igsh"),      QStringLiteral("igshid"),     QStringLiteral("fbclid"),  QStringLiteral("gclid"),
        QStringLiteral("gclsrc"),     QStringLiteral("dclid"),     QStringLiteral("gad_source"), QStringLiteral("gbraid"),  QStringLiteral("wbraid"),
        QStringLiteral("twclid"),     QStringLiteral("ref_src"),   QStringLiteral("ref_url"),    QStringLiteral("msclkid"), QStringLiteral("cvid"),
        QStringLiteral("mc_eid"),     QStringLiteral("mc_cid"),    QStringLiteral("mkt_tok"),    QStringLiteral("trk"),     QStringLiteral("tracking_id"),
        QStringLiteral("spm"),        QStringLiteral("scm"),       QStringLiteral("_t"),         QStringLiteral("_r"),      QStringLiteral("tt_medium"),
        QStringLiteral("tt_content"), QStringLiteral("tt_source"), QStringLiteral("_ga"),        QStringLiteral("_gl"),     QStringLiteral("yclid"),
        QStringLiteral("zanpid"),     QStringLiteral("vero_id"),   QStringLiteral("vero_conv"),  QStringLiteral("wickedid")};

    const QString host = url.host().toLower();
    const bool isTwitter = host.contains(QLatin1String("twitter.com")) || host.contains(QLatin1String("x.com"));
    const bool isYoutube = host.contains(QLatin1String("youtube.com")) || host.contains(QLatin1String("youtu.be"));
    const bool isAmazon = host.contains(QLatin1String("amazon."));

    const auto items = query.queryItems(QUrl::FullyDecoded);
    QUrlQuery newQuery;
    int localRemoved = 0;

    for (const auto &item : items) {
        const QString lowerKey = item.first.toLower();

        bool isTracker = false;
        if (lowerKey.startsWith(QLatin1String("utm_")) || lowerKey.startsWith(QLatin1String("ref_")) || lowerKey.startsWith(QLatin1String("pf_rd_"))
            || lowerKey.startsWith(QLatin1String("pd_rd_")) || lowerKey.startsWith(QLatin1String("fb_")) || lowerKey.startsWith(QLatin1String("ga_"))
            || lowerKey.startsWith(QLatin1String("mc_"))) {
            isTracker = true;
        } else if (exactTrackers.contains(lowerKey)) {
            isTracker = true;
        } else if (isTwitter && (lowerKey == QLatin1String("s") || lowerKey == QLatin1String("t"))) {
            isTracker = true;
        } else if (isYoutube && lowerKey == QLatin1String("feature")) {
            isTracker = true;
        } else if (isAmazon && (lowerKey == QLatin1String("tag") || lowerKey == QLatin1String("linkcode"))) {
            isTracker = true;
        }

        if (isTracker) {
            localRemoved++;
        } else {
            newQuery.addQueryItem(item.first, item.second);
        }
    }

    if (localRemoved > 0) {
        removedCount += localRemoved;
        QUrl clean = url;
        if (newQuery.isEmpty()) {
            clean.setQuery(QString());
        } else {
            clean.setQuery(newQuery);
        }
        return clean;
    }

    return url;
}

QString ClipboardPlugin::sanitizeTracking(const QString &text, bool &wasSanitized, int &strippedCount)
{
    wasSanitized = false;
    strippedCount = 0;

    if (text.trimmed().isEmpty()) {
        return text;
    }

    if (!text.contains(QLatin1String("http://")) && !text.contains(QLatin1String("https://"))) {
        return text;
    }

    // Direct single URL test
    const QString trimmed = text.trimmed();
    QUrl directUrl(trimmed);
    if (directUrl.isValid() && (directUrl.scheme() == QLatin1String("http") || directUrl.scheme() == QLatin1String("https"))
        && !trimmed.contains(QLatin1Char(' ')) && !trimmed.contains(QLatin1Char('\n'))) {
        int removed = 0;
        QUrl clean = cleanSingleUrl(directUrl, removed);
        if (removed > 0) {
            wasSanitized = true;
            strippedCount = removed;
            return clean.toString();
        }
        return text;
    }

    // Embedded URLs matching
    static const QRegularExpression urlRegex(QStringLiteral(R"((https?://[^\s<>"'{}|\\^`]+))"), QRegularExpression::CaseInsensitiveOption);

    QString result;
    int lastPos = 0;
    auto it = urlRegex.globalMatch(text);

    while (it.hasNext()) {
        auto match = it.next();
        result.append(text.mid(lastPos, match.capturedStart() - lastPos));

        QString rawUrl = match.captured(1);
        QString trailingPunct;
        while (!rawUrl.isEmpty()
               && (rawUrl.endsWith(QLatin1Char('.')) || rawUrl.endsWith(QLatin1Char(',')) || rawUrl.endsWith(QLatin1Char(';'))
                   || rawUrl.endsWith(QLatin1Char(':')) || rawUrl.endsWith(QLatin1Char('!')) || rawUrl.endsWith(QLatin1Char('?'))
                   || rawUrl.endsWith(QLatin1Char(')')) || rawUrl.endsWith(QLatin1Char(']')))) {
            trailingPunct.prepend(rawUrl.at(rawUrl.length() - 1));
            rawUrl.chop(1);
        }

        QUrl parsedUrl(rawUrl);
        int removed = 0;
        if (parsedUrl.isValid()) {
            QUrl cleaned = cleanSingleUrl(parsedUrl, removed);
            if (removed > 0) {
                wasSanitized = true;
                strippedCount += removed;
                result.append(cleaned.toString());
            } else {
                result.append(rawUrl);
            }
        } else {
            result.append(rawUrl);
        }
        result.append(trailingPunct);
        lastPos = match.capturedEnd();
    }

    result.append(text.mid(lastPos));
    return result;
}

void ClipboardPlugin::performOcrOnClipboardImage(const QString &imagePath)
{
    QFileInfo fi(imagePath);
    if (!fi.exists() || !fi.isFile()) {
        return;
    }

    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForFile(imagePath);
    if (!mime.name().startsWith(QLatin1String("image/"))) {
        return;
    }

    QString tesseractBin = QStandardPaths::findExecutable(QStringLiteral("tesseract"));
    if (tesseractBin.isEmpty()) {
        if (QFileInfo::exists(QStringLiteral("/usr/bin/tesseract"))) {
            tesseractBin = QStringLiteral("/usr/bin/tesseract");
        } else {
            qCDebug(KDECONNECT_PLUGIN_CLIPBOARD) << "Tesseract executable not found, skipping OCR";
            return;
        }
    }

    QString languages = QStringLiteral("eng");
    if (QFileInfo::exists(QStringLiteral("/usr/share/tessdata/spa.traineddata"))) {
        languages = QStringLiteral("spa+eng");
    }

    auto *process = new QProcess(this);
    connect(process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            [this, process, imagePath](int exitCode, QProcess::ExitStatus exitStatus) {
                process->deleteLater();
                if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
                    const QString text = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
                    if (!text.isEmpty()) {
                        handleOcrResult(text, imagePath);
                    }
                }
            });

    process->start(tesseractBin,
                   {imagePath,
                    QStringLiteral("stdout"),
                    QStringLiteral("--oem"),
                    QStringLiteral("1"),
                    QStringLiteral("-l"),
                    languages,
                    QStringLiteral("--loglevel"),
                    QStringLiteral("ERROR")});
}

void ClipboardPlugin::handleOcrResult(const QString &text, const QString &imagePath)
{
    qCDebug(KDECONNECT_PLUGIN_CLIPBOARD) << "OCR detected" << text.length() << "characters from image" << imagePath;
    Q_EMIT ocrTextDetected(text);

    if (autoCopyOcrText) {
        ClipboardListener::instance()->setText(text);
    }

    auto *notif = new KNotification(QStringLiteral("notification"), KNotification::CloseOnTimeout);
    notif->setComponentName(QStringLiteral("kdeconnect"));
    notif->setTitle(i18n("Text Detected in Image"));

    QString preview = text;
    if (preview.length() > 140) {
        preview = preview.left(137) + QStringLiteral("...");
    }
    notif->setText(preview);
    notif->setIconName(QStringLiteral("edit-copy"));

    auto *copyAction = notif->addAction(i18nc("@action:button Copy text to clipboard", "Copy Text"));
    connect(copyAction, &KNotificationAction::activated, this, [text, notif]() {
        ClipboardListener::instance()->setText(text);
        notif->close();
    });

    notif->sendEvent();
}

#include "clipboardplugin.moc"
#include "moc_clipboardplugin.cpp"
