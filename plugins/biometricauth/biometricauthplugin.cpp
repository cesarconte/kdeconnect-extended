/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "biometricauthplugin.h"

#include <core/daemon.h>
#include <core/device.h>
#include <core/networkpacket.h>

#include <KLocalizedString>
#include <KNotification>
#include <KPluginFactory>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHostInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>
#include <sys/stat.h>

K_PLUGIN_CLASS_WITH_JSON(BiometricAuthPlugin, "kdeconnect_biometricauth.json")

BiometricAuthPlugin::BiometricAuthPlugin(QObject *parent, const QVariantList &args)
    : KdeConnectPlugin(parent, args)
{
    setupSocketServer();
    connect(&m_timeoutTimer, &QTimer::timeout, this, &BiometricAuthPlugin::handleAuthTimeout);
    m_timeoutTimer.setSingleShot(true);
    registerRunCommands();
}

BiometricAuthPlugin::~BiometricAuthPlugin()
{
    if (m_server) {
        m_server->close();
        delete m_server;
        m_server = nullptr;
    }
}

bool BiometricAuthPlugin::isPending() const
{
    return m_isPending;
}

QString BiometricAuthPlugin::lastPrompt() const
{
    return m_lastPrompt;
}

QString BiometricAuthPlugin::currentCommand() const
{
    return m_currentCommand;
}

QString BiometricAuthPlugin::currentService() const
{
    return m_currentService;
}

QString BiometricAuthPlugin::currentRiskLevel() const
{
    return m_currentRiskLevel;
}

QString BiometricAuthPlugin::currentTransactionId() const
{
    return m_currentTransactionId;
}

QString BiometricAuthPlugin::calculateRiskLevel(const QString &command) const
{
    const QString cmd = command.toLower().trimmed();
    if (cmd.contains(QLatin1String("dd ")) || cmd.contains(QLatin1String("mkfs")) || cmd.contains(QLatin1String("fdisk"))
        || cmd.contains(QLatin1String("parted")) || cmd.contains(QLatin1String("wipefs")) || cmd.contains(QLatin1String("rm -rf"))
        || cmd.contains(QLatin1String("shred")) || cmd.contains(QLatin1String("format"))) {
        return QStringLiteral("critical");
    }
    if (cmd.contains(QLatin1String("pacman -r")) || cmd.contains(QLatin1String("apt remove")) || cmd.contains(QLatin1String("systemctl stop"))
        || cmd.contains(QLatin1String("reboot")) || cmd.contains(QLatin1String("poweroff")) || cmd.contains(QLatin1String("shutdown"))) {
        return QStringLiteral("high");
    }
    return QStringLiteral("medium");
}

void BiometricAuthPlugin::setupSocketServer()
{
    if (m_server) {
        return;
    }

    QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtimeDir.isEmpty()) {
        runtimeDir = QStringLiteral("/tmp");
    }
    const QString socketPath = runtimeDir + QStringLiteral("/kdeconnect_auth.sock");

    QLocalServer::removeServer(socketPath);

    m_server = new QLocalServer(this);
    connect(m_server, &QLocalServer::newConnection, this, &BiometricAuthPlugin::handleNewConnection);

    if (m_server->listen(socketPath)) {
        chmod(socketPath.toUtf8().constData(), S_IRUSR | S_IWUSR);
    }
}

void BiometricAuthPlugin::handleNewConnection()
{
    if (!m_server) {
        return;
    }

    QLocalSocket *socket = m_server->nextPendingConnection();
    if (!socket) {
        return;
    }

    if (m_currentSocket && m_currentSocket->isOpen()) {
        // Rechazar si ya hay una autenticación en curso
        QJsonObject rej;
        rej[QStringLiteral("status")] = QStringLiteral("busy");
        rej[QStringLiteral("message")] = i18n("Otra solicitud de autenticación está en curso.");
        socket->write(QJsonDocument(rej).toJson(QJsonDocument::Compact) + "\n");
        socket->flush();
        socket->disconnectFromServer();
        socket->deleteLater();
        return;
    }

    m_currentSocket = socket;
    connect(m_currentSocket, &QLocalSocket::readyRead, this, &BiometricAuthPlugin::handleSocketData);
    connect(m_currentSocket, &QLocalSocket::disconnected, this, [this]() {
        if (m_currentSocket) {
            m_currentSocket->deleteLater();
            m_currentSocket = nullptr;
        }
    });
}

void BiometricAuthPlugin::handleSocketData()
{
    if (!m_currentSocket || !m_currentSocket->canReadLine()) {
        return;
    }

    const QByteArray line = m_currentSocket->readLine().trimmed();
    const QJsonDocument doc = QJsonDocument::fromJson(line);
    if (!doc.isObject()) {
        return;
    }

    const QJsonObject obj = doc.object();
    const QString action = obj.value(QStringLiteral("action")).toString();
    const QString user = obj.value(QStringLiteral("user")).toString();
    const QString service = obj.value(QStringLiteral("service")).toString(QStringLiteral("sudo"));
    const QString command = obj.value(QStringLiteral("command")).toString();
    QString risk = obj.value(QStringLiteral("riskLevel")).toString();
    if (risk.isEmpty()) {
        risk = calculateRiskLevel(command);
    }

    if (action != QLatin1String("authenticate") && action != QLatin1String("authorize") && action != QLatin1String("dualkey")) {
        sendResponseToPam(QStringLiteral("error"), QStringLiteral("Acción no válida"));
        return;
    }

    // Verificar si el dispositivo está emparejado y accesible
    if (!device()->isReachable()) {
        sendResponseToPam(QStringLiteral("unavailable"), QStringLiteral("Dispositivo remoto no accesible"));
        return;
    }

    m_currentTransactionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_currentCommand = command;
    m_currentService = service;
    m_currentRiskLevel = risk;
    m_isPending = true;

    if (!m_currentCommand.isEmpty()) {
        m_lastPrompt = i18n("🔑 Dual-Key: Se requiere autorización para ejecutar '%1' (%2 - %3)", m_currentCommand, user, service);
    } else {
        m_lastPrompt = i18n("Autenticación requerida para %1 (%2)", user, service);
    }

    // Notificar al cliente socket que estamos esperando confirmación en el móvil
    sendResponseToPam(QStringLiteral("prompt"), m_lastPrompt);

    // Enviar paquete de autenticación biométrica y dual-key al móvil
    NetworkPacket req(PACKET_TYPE_BIOMETRIC_REQUEST);
    req.set(QStringLiteral("transactionId"), m_currentTransactionId);
    req.set(QStringLiteral("user"), user);
    req.set(QStringLiteral("service"), service);
    req.set(QStringLiteral("command"), m_currentCommand);
    req.set(QStringLiteral("riskLevel"), m_currentRiskLevel);
    req.set(QStringLiteral("hostname"), QHostInfo::localHostName());
    req.set<qint64>(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());
    sendPacket(req);

    // Enviar vibración háptica al dispositivo
    NetworkPacket ping(PACKET_TYPE_PING);
    sendPacket(ping);

    // Enviar notificación visible al móvil (en pantalla bloqueada y barra de notificaciones)
    sendMobileNotification(service, m_currentCommand, m_currentRiskLevel);

    // Mostrar aviso en el escritorio
    notifyDesktop(service, m_currentCommand, m_currentRiskLevel);

    // Iniciar tiempo límite de 25 segundos
    m_timeoutTimer.stop();
    m_timeoutTimer.start(25000);

    Q_EMIT pendingChanged();
    Q_EMIT authRequested(m_currentTransactionId, service, m_currentCommand, m_currentRiskLevel);
}

void BiometricAuthPlugin::sendResponseToPam(const QString &status, const QString &message)
{
    if (!m_currentSocket || !m_currentSocket->isOpen()) {
        return;
    }

    QJsonObject resp;
    resp[QStringLiteral("status")] = status;
    if (!message.isEmpty()) {
        resp[QStringLiteral("message")] = message;
    }

    m_currentSocket->write(QJsonDocument(resp).toJson(QJsonDocument::Compact) + "\n");
    m_currentSocket->flush();

    if (status == QLatin1String("success") || status == QLatin1String("rejected") || status == QLatin1String("unavailable")
        || status == QLatin1String("timeout") || status == QLatin1String("error")) {
        m_currentSocket->disconnectFromServer();
    }
}

void BiometricAuthPlugin::notifyDesktop(const QString &service, const QString &command, const QString &riskLevel)
{
    auto *notif = new KNotification(QStringLiteral("notification"), KNotification::CloseOnTimeout);
    notif->setIconName(riskLevel == QLatin1String("critical") ? QStringLiteral("security-low") : QStringLiteral("security-high"));
    notif->setComponentName(QStringLiteral("kdeconnect"));

    if (!command.isEmpty()) {
        notif->setTitle(i18n("🔑 Dual-Key: Autorización Requerida"));
        notif->setText(i18n("Comando: %1\nServicio: %2 [%3]\nEsperando confirmación en %4...", command, service, riskLevel.toUpper(), device()->name()));
    } else {
        notif->setTitle(i18n("Autenticación solicitada"));
        notif->setText(i18n("Solicitud de %1: esperando confirmación en %2", service, device()->name()));
    }

    auto *approveAction = notif->addAction(i18n("Aprobar"));
    connect(approveAction, &KNotificationAction::activated, this, [this, notif] {
        respondBiometric(true);
        notif->close();
    });

    auto *rejectAction = notif->addAction(i18n("Rechazar"));
    connect(rejectAction, &KNotificationAction::activated, this, [this, notif] {
        respondBiometric(false);
        notif->close();
    });

    notif->sendEvent();
}

void BiometricAuthPlugin::handleAuthTimeout()
{
    if (!m_isPending) {
        return;
    }

    m_isPending = false;
    cancelMobileNotification();
    sendResponseToPam(QStringLiteral("timeout"), i18n("Tiempo límite de autenticación agotado"));
    Q_EMIT authenticationFinished(false);

    m_currentCommand.clear();
    m_currentService.clear();
    m_currentRiskLevel.clear();
    m_currentTransactionId.clear();

    Q_EMIT pendingChanged();
}

void BiometricAuthPlugin::respondBiometric(bool accepted)
{
    if (!m_isPending) {
        return;
    }

    m_timeoutTimer.stop();
    m_isPending = false;
    cancelMobileNotification();

    if (accepted) {
        sendResponseToPam(QStringLiteral("success"), i18n("Autenticado biométricamente con éxito"));
        Q_EMIT authenticationFinished(true);
    } else {
        sendResponseToPam(QStringLiteral("rejected"), i18n("Autenticación biométrica rechazada"));
        Q_EMIT authenticationFinished(false);
    }

    m_currentCommand.clear();
    m_currentService.clear();
    m_currentRiskLevel.clear();
    m_currentTransactionId.clear();

    Q_EMIT pendingChanged();
}

void BiometricAuthPlugin::cancelBiometric()
{
    respondBiometric(false);
}

void BiometricAuthPlugin::requestBiometric(const QString &service)
{
    requestAuthorization(service, QString(), QStringLiteral("medium"));
}

QString BiometricAuthPlugin::requestAuthorization(const QString &service, const QString &command, const QString &riskLevel)
{
    if (!device()->isReachable()) {
        return QString();
    }

    m_currentTransactionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_currentCommand = command;
    m_currentService = service;
    m_currentRiskLevel = riskLevel.isEmpty() ? calculateRiskLevel(command) : riskLevel;
    m_isPending = true;

    if (!m_currentCommand.isEmpty()) {
        m_lastPrompt = i18n("🔑 Dual-Key: Se requiere autorización para '%1' (%2)", m_currentCommand, service);
    } else {
        m_lastPrompt = i18n("Autenticación requerida para %1", service);
    }

    NetworkPacket req(PACKET_TYPE_BIOMETRIC_REQUEST);
    req.set(QStringLiteral("transactionId"), m_currentTransactionId);
    req.set(QStringLiteral("user"), QString::fromLocal8Bit(qgetenv("USER")));
    req.set(QStringLiteral("service"), service);
    req.set(QStringLiteral("command"), m_currentCommand);
    req.set(QStringLiteral("riskLevel"), m_currentRiskLevel);
    req.set(QStringLiteral("hostname"), QHostInfo::localHostName());
    req.set<qint64>(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());
    sendPacket(req);

    NetworkPacket ping(PACKET_TYPE_PING);
    sendPacket(ping);

    sendMobileNotification(service, m_currentCommand, m_currentRiskLevel);

    notifyDesktop(service, m_currentCommand, m_currentRiskLevel);

    m_timeoutTimer.stop();
    m_timeoutTimer.start(25000);

    Q_EMIT pendingChanged();
    Q_EMIT authRequested(m_currentTransactionId, service, m_currentCommand, m_currentRiskLevel);

    return m_currentTransactionId;
}

void BiometricAuthPlugin::sendMobileNotification(const QString &service, const QString &command, const QString &riskLevel)
{
    NetworkPacket mobileNotif(QStringLiteral("kdeconnect.notification"));
    mobileNotif.set(QStringLiteral("id"), QStringLiteral("dualkey_auth_request"));
    mobileNotif.set(QStringLiteral("appName"), QStringLiteral("KDE Connect Dual-Key"));
    mobileNotif.set(QStringLiteral("title"), i18n("🔑 Dual-Key: Autorización Requerida [%1]", riskLevel.toUpper()));
    const QString cmdText = !command.isEmpty() ? command : service;
    const QString bodyText = i18n("Comando: %1\nToca 'Aprobar Dual-Key' en Ejecutar Órdenes o abre Plasma Deck.", cmdText);
    mobileNotif.set(QStringLiteral("text"), bodyText);
    mobileNotif.set(QStringLiteral("ticker"), i18n("🔑 Dual-Key: Autorización Requerida"));
    mobileNotif.set(QStringLiteral("isClearable"), true);
    mobileNotif.set(QStringLiteral("silent"), false);
    sendPacket(mobileNotif);
}

void BiometricAuthPlugin::cancelMobileNotification()
{
    NetworkPacket cancelNotif(QStringLiteral("kdeconnect.notification"));
    cancelNotif.set(QStringLiteral("id"), QStringLiteral("dualkey_auth_request"));
    cancelNotif.set(QStringLiteral("isCancel"), true);
    sendPacket(cancelNotif);
}

void BiometricAuthPlugin::registerRunCommands()
{
    if (!device()) {
        return;
    }

    const QString configPath =
        QStringLiteral("%1/%2/kdeconnect_runcommand/config").arg(QDir::homePath() + QStringLiteral("/.config/kdeconnect")).arg(device()->id());

    QSettings s(configPath, QSettings::IniFormat);
    QJsonObject json = QJsonDocument::fromJson(s.value(QStringLiteral("commands")).toByteArray()).object();

    QJsonObject approveEntry;
    approveEntry[QStringLiteral("name")] = QString::fromUtf8("✅ Aprobar Dual-Key");
    approveEntry[QStringLiteral("command")] =
        QStringLiteral(
            "/usr/bin/qdbus6 org.kde.kdeconnect /modules/kdeconnect/devices/%1/biometricauth org.kde.kdeconnect.device.biometricauth.respondBiometric true")
            .arg(device()->id());

    QJsonObject rejectEntry;
    rejectEntry[QStringLiteral("name")] = QString::fromUtf8("❌ Rechazar Dual-Key");
    rejectEntry[QStringLiteral("command")] =
        QStringLiteral(
            "/usr/bin/qdbus6 org.kde.kdeconnect /modules/kdeconnect/devices/%1/biometricauth org.kde.kdeconnect.device.biometricauth.respondBiometric false")
            .arg(device()->id());

    json[QStringLiteral("dualkey_approve")] = approveEntry;
    json[QStringLiteral("dualkey_reject")] = rejectEntry;

    QJsonDocument doc(json);
    s.setValue(QStringLiteral("commands"), doc.toJson(QJsonDocument::Compact));
    s.sync();
}

void BiometricAuthPlugin::connected()
{
    registerRunCommands();
}

void BiometricAuthPlugin::receivePacket(const NetworkPacket &np)
{
    if (np.type() != PACKET_TYPE_BIOMETRIC_RESPONSE) {
        return;
    }

    const QString tid = np.get<QString>(QStringLiteral("transactionId"));
    if (!m_currentTransactionId.isEmpty() && tid != m_currentTransactionId) {
        return;
    }

    const bool accepted = np.get<bool>(QStringLiteral("accepted"), false);
    respondBiometric(accepted);
}

QString BiometricAuthPlugin::dbusPath() const
{
    return QStringLiteral("/modules/kdeconnect/devices/%1/biometricauth").arg(device()->id());
}

#include "biometricauthplugin.moc"
#include "moc_biometricauthplugin.cpp"
