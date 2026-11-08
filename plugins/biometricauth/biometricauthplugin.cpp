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
#include <QFile>
#include <QHostInfo>
#include <QJsonDocument>
#include <QJsonObject>
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

    if (action != QLatin1String("authenticate")) {
        sendResponseToPam(QStringLiteral("error"), QStringLiteral("Acción no válida"));
        return;
    }

    // Verificar si el dispositivo está emparejado y accesible
    if (!device()->isReachable()) {
        sendResponseToPam(QStringLiteral("unavailable"), QStringLiteral("Dispositivo remoto no accesible"));
        return;
    }

    m_currentTransactionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_lastPrompt = i18n("Autenticación requerida para %1 (%2)", user, service);
    m_isPending = true;

    // Notificar al módulo PAM que estamos esperando confirmación en el móvil
    sendResponseToPam(QStringLiteral("prompt"), m_lastPrompt);

    // Enviar paquete de autenticación biométrica al móvil
    NetworkPacket req(PACKET_TYPE_BIOMETRIC_REQUEST);
    req.set(QStringLiteral("transactionId"), m_currentTransactionId);
    req.set(QStringLiteral("user"), user);
    req.set(QStringLiteral("service"), service);
    req.set(QStringLiteral("hostname"), QHostInfo::localHostName());
    req.set<qint64>(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());
    sendPacket(req);

    // Enviar vibración háptica al dispositivo
    NetworkPacket ping(PACKET_TYPE_PING);
    sendPacket(ping);

    // Mostrar aviso en el escritorio
    notifyDesktop(service);

    // Iniciar tiempo límite de 25 segundos
    m_timeoutTimer.stop();
    m_timeoutTimer.start(25000);

    Q_EMIT pendingChanged();
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

void BiometricAuthPlugin::notifyDesktop(const QString &service)
{
    auto *notif = new KNotification(QStringLiteral("notification"), KNotification::CloseOnTimeout);
    notif->setIconName(QStringLiteral("security-high"));
    notif->setComponentName(QStringLiteral("kdeconnect"));
    notif->setTitle(i18n("Autenticación solicitada"));
    notif->setText(i18n("Solicitud de %1: esperando confirmación en %2", service, device()->name()));

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
    sendResponseToPam(QStringLiteral("timeout"), i18n("Tiempo límite de autenticación agotado"));
    Q_EMIT authenticationFinished(false);
    Q_EMIT pendingChanged();
}

void BiometricAuthPlugin::respondBiometric(bool accepted)
{
    if (!m_isPending) {
        return;
    }

    m_timeoutTimer.stop();
    m_isPending = false;

    if (accepted) {
        sendResponseToPam(QStringLiteral("success"), i18n("Autenticado biométricamente con éxito"));
        Q_EMIT authenticationFinished(true);
    } else {
        sendResponseToPam(QStringLiteral("rejected"), i18n("Autenticación biométrica rechazada"));
        Q_EMIT authenticationFinished(false);
    }

    Q_EMIT pendingChanged();
}

void BiometricAuthPlugin::cancelBiometric()
{
    respondBiometric(false);
}

void BiometricAuthPlugin::requestBiometric(const QString &service)
{
    Q_UNUSED(service);
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
