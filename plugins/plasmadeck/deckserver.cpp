/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "deckserver.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QUrl>
#include <QUrlQuery>

DeckServer::DeckServer(quint16 port, QObject *parent)
    : QObject(parent)
    , m_port(port)
{
    connect(&m_server, &QTcpServer::newConnection, this, &DeckServer::onNewConnection);
}

DeckServer::~DeckServer()
{
    stop();
}

bool DeckServer::start()
{
    if (m_server.isListening()) {
        return true;
    }

    if (!m_server.listen(QHostAddress::AnyIPv4, m_port)) {
        qWarning() << "DeckServer failed to listen on port" << m_port << ":" << m_server.errorString();
        return false;
    }

    qDebug() << "DeckServer listening on port" << m_port;
    return true;
}

void DeckServer::stop()
{
    for (auto *client : std::as_const(m_sseClients)) {
        if (client) {
            client->disconnectFromHost();
            client->deleteLater();
        }
    }
    m_sseClients.clear();

    if (m_server.isListening()) {
        m_server.close();
    }
}

quint16 DeckServer::port() const
{
    return m_port;
}

QString DeckServer::authToken() const
{
    return m_authToken;
}

void DeckServer::setAuthToken(const QString &token)
{
    m_authToken = token;
}

QStringList DeckServer::allowedAddresses() const
{
    return m_allowedAddresses;
}

void DeckServer::setAllowedAddresses(const QStringList &addresses)
{
    m_allowedAddresses = addresses;
}

QString DeckServer::serverUrl(const QString &hostIp) const
{
    if (m_authToken.isEmpty()) {
        return QStringLiteral("http://%1:%2/").arg(hostIp).arg(m_port);
    }
    return QStringLiteral("http://%1:%2/?token=%3").arg(hostIp).arg(m_port).arg(m_authToken);
}

bool DeckServer::isClientIpAllowed(const QHostAddress &addr) const
{
    if (m_allowedAddresses.isEmpty()) {
        return true;
    }

    // Permitir siempre localhost y loopback
    if (addr.isLoopback() || addr == QHostAddress::LocalHost || addr == QHostAddress::LocalHostIPv6 || addr == QHostAddress(QStringLiteral("127.0.0.1"))
        || addr == QHostAddress(QStringLiteral("::1"))) {
        return true;
    }

    const QString clientIpStr = addr.toString();
    for (const auto &allowed : m_allowedAddresses) {
        if (allowed.isEmpty()) {
            continue;
        }
        if (addr == QHostAddress(allowed) || clientIpStr.endsWith(allowed)) {
            return true;
        }
    }

    return false;
}

bool DeckServer::isRequestAuthorized(QTcpSocket *socket, const QString &path, const QString &reqHeaders, const QByteArray &body) const
{
    Q_UNUSED(socket)
    if (m_authToken.isEmpty()) {
        return true;
    }

    // 1. Query parameter
    QUrl url(path);
    QUrlQuery query(url);
    if (query.hasQueryItem(QStringLiteral("token")) && query.queryItemValue(QStringLiteral("token")) == m_authToken) {
        return true;
    }

    // 2. Cabecera personalizada X-Deck-Token
    const QString tokenHeader = QStringLiteral("X-Deck-Token: %1").arg(m_authToken);
    if (reqHeaders.contains(tokenHeader, Qt::CaseInsensitive)) {
        return true;
    }

    // 3. Authorization Bearer
    const QString bearerHeader = QStringLiteral("Authorization: Bearer %1").arg(m_authToken);
    if (reqHeaders.contains(bearerHeader, Qt::CaseInsensitive)) {
        return true;
    }

    // 4. Cookie de sesión
    const QString cookieToken = QStringLiteral("deck_token=%1").arg(m_authToken);
    if (reqHeaders.contains(cookieToken)) {
        return true;
    }

    // 5. Cuerpo JSON
    if (!body.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(body);
        if (doc.isObject() && doc.object().value(QStringLiteral("token")).toString() == m_authToken) {
            return true;
        }
    }

    return false;
}

void DeckServer::onNewConnection()
{
    while (m_server.hasPendingConnections()) {
        QTcpSocket *socket = m_server.nextPendingConnection();
        if (!socket) {
            continue;
        }

        connect(socket, &QTcpSocket::readyRead, this, &DeckServer::onClientDataReady);
        connect(socket, &QTcpSocket::disconnected, this, &DeckServer::onClientDisconnected);
    }
}

void DeckServer::onClientDataReady()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket) {
        return;
    }

    const QByteArray data = socket->readAll();
    handleHttpRequest(socket, data);
}

void DeckServer::onClientDisconnected()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket) {
        return;
    }

    m_sseClients.removeAll(socket);
    socket->deleteLater();
}

void DeckServer::handleHttpRequest(QTcpSocket *socket, const QByteArray &data)
{
    if (data.isEmpty()) {
        return;
    }

    // 1. Validación de IP
    if (!isClientIpAllowed(socket->peerAddress())) {
        sendResponse(socket, 403, QStringLiteral("text/plain"), "403 Forbidden: IP no autorizada");
        return;
    }

    const QString req = QString::fromUtf8(data);
    const QString firstLine = req.section(QLatin1Char('\n'), 0, 0).trimmed();
    const QStringList parts = firstLine.split(QLatin1Char(' '));
    if (parts.size() < 2) {
        return;
    }

    const QString method = parts[0];
    const QString path = parts[1];

    int bodyIdx = data.indexOf("\r\n\r\n");
    if (bodyIdx == -1) {
        bodyIdx = data.indexOf("\n\n");
    }
    const QString reqHeaders = (bodyIdx != -1) ? QString::fromUtf8(data.left(bodyIdx)) : req;
    const QByteArray body = (bodyIdx != -1) ? data.mid(bodyIdx + ((data.contains("\r\n\r\n")) ? 4 : 2)) : QByteArray();

    // Extraer origen para protección CORS / CSRF
    QString originHeader;
    const auto headerLines = reqHeaders.split(QLatin1Char('\n'));
    for (const auto &h : headerLines) {
        if (h.startsWith(QLatin1String("Origin:"), Qt::CaseInsensitive)) {
            originHeader = h.section(QLatin1Char(':'), 1).trimmed();
            break;
        }
    }

    if (method == QLatin1String("OPTIONS")) {
        sendResponse(socket, 204, QStringLiteral("text/plain"), QByteArray(), originHeader);
        return;
    }

    // 2. Validación de Token Criptográfico
    if (!isRequestAuthorized(socket, path, reqHeaders, body)) {
        sendResponse(socket,
                     401,
                     QStringLiteral("text/html; charset=utf-8"),
                     "<!DOCTYPE html><html><body style='background:#0a0a10;color:#f87171;font-family:sans-serif;text-align:center;padding:50px;'>"
                     "<h2>401 No Autorizado</h2><p style='color:#8e8e9f'>Token de seguridad de Plasma Deck no proporcionado o inválido.</p>"
                     "</body></html>",
                     originHeader);
        return;
    }

    if ((method == QLatin1String("GET") || method == QLatin1String("HEAD"))
        && (path == QLatin1String("/") || path == QLatin1String("/index.html") || path.startsWith(QLatin1String("/?"))
            || path.startsWith(QLatin1String("/index.html?")))) {
        sendResponse(socket, 200, QStringLiteral("text/html; charset=utf-8"), (method == QLatin1String("HEAD")) ? QByteArray() : getIndexHtml(), originHeader);
        return;
    }

    if (method == QLatin1String("GET") && (path == QLatin1String("/api/events") || path.startsWith(QLatin1String("/api/events?")))) {
        // Registrar cliente Server-Sent Events (SSE)
        QByteArray sseHeader =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: keep-alive\r\n";
        if (!originHeader.isEmpty()) {
            if (originHeader.contains(QStringLiteral("localhost")) || originHeader.contains(QStringLiteral("127.0.0.1"))
                || originHeader.contains(QString::number(m_port))) {
                sseHeader += "Access-Control-Allow-Origin: " + originHeader.toUtf8() + "\r\n";
                sseHeader += "Access-Control-Allow-Credentials: true\r\n";
            }
        }
        sseHeader += "\r\n";
        socket->write(sseHeader);
        socket->flush();

        if (!m_sseClients.contains(socket)) {
            m_sseClients.append(socket);
        }

        // Enviar estado actual inmediato al cliente
        QJsonObject initTele;
        initTele[QStringLiteral("type")] = QStringLiteral("telemetry");
        initTele[QStringLiteral("cpu")] = m_lastCpu;
        initTele[QStringLiteral("ram")] = m_lastRam;
        initTele[QStringLiteral("temp")] = m_lastTemp;
        QByteArray teleMsg = "data: " + QJsonDocument(initTele).toJson(QJsonDocument::Compact) + "\n\n";
        socket->write(teleMsg);

        QJsonObject initCtx;
        initCtx[QStringLiteral("type")] = QStringLiteral("context");
        initCtx[QStringLiteral("app")] = m_lastAppName;
        initCtx[QStringLiteral("title")] = m_lastWindowTitle;
        initCtx[QStringLiteral("buttons")] = m_lastButtons;
        QByteArray ctxMsg = "data: " + QJsonDocument(initCtx).toJson(QJsonDocument::Compact) + "\n\n";
        socket->write(ctxMsg);
        socket->flush();
        return;
    }

    if (method == QLatin1String("POST") && (path == QLatin1String("/api/action") || path.startsWith(QLatin1String("/api/action?")))) {
        QJsonDocument doc = QJsonDocument::fromJson(body);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            const QString id = obj.value(QStringLiteral("id")).toString();
            const QString cmd = obj.value(QStringLiteral("command")).toString();
            const QString shortcut = obj.value(QStringLiteral("shortcut")).toString();
            Q_EMIT actionTriggered(id, cmd, shortcut);
        }

        QByteArray resp = "{\"status\":\"ok\"}";
        sendResponse(socket, 200, QStringLiteral("application/json"), resp, originHeader);
        return;
    }

    if (method == QLatin1String("POST") && (path == QLatin1String("/api/spatial") || path.startsWith(QLatin1String("/api/spatial?")))) {
        QJsonDocument doc = QJsonDocument::fromJson(body);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            const QString type = obj.value(QStringLiteral("type")).toString();
            QString val = obj.value(QStringLiteral("value")).toString();
            if (val.isEmpty()) {
                val = obj.value(QStringLiteral("gesture")).toString();
            }
            if (val.isEmpty()) {
                val = obj.value(QStringLiteral("state")).toString();
            }
            const int durationMs = obj.contains(QStringLiteral("durationMs")) ? obj.value(QStringLiteral("durationMs")).toInt()
                                                                              : obj.value(QStringLiteral("duration_ms")).toInt(0);
            Q_EMIT spatialEventReceived(type, val, durationMs);
        }

        QByteArray resp = "{\"status\":\"ok\"}";
        sendResponse(socket, 200, QStringLiteral("application/json"), resp, originHeader);
        return;
    }

    // 404 por defecto
    sendResponse(socket, 404, QStringLiteral("text/plain"), "Not Found", originHeader);
}

void DeckServer::sendResponse(QTcpSocket *socket, int statusCode, const QString &contentType, const QByteArray &body, const QString &origin)
{
    QString statusText = (statusCode == 200) ? QStringLiteral("OK")
        : (statusCode == 204)                ? QStringLiteral("No Content")
        : (statusCode == 401)                ? QStringLiteral("Unauthorized")
        : (statusCode == 403)                ? QStringLiteral("Forbidden")
                                             : QStringLiteral("Not Found");
    QByteArray header;
    header.append(QStringLiteral("HTTP/1.1 %1 %2\r\n").arg(statusCode).arg(statusText).toUtf8());
    header.append(QStringLiteral("Content-Type: %1\r\n").arg(contentType).toUtf8());
    header.append(QStringLiteral("Content-Length: %1\r\n").arg(body.size()).toUtf8());

    // Token Session Cookie (SameSite=Strict, HttpOnly)
    if (!m_authToken.isEmpty() && statusCode == 200) {
        header.append(QStringLiteral("Set-Cookie: deck_token=%1; Path=/; SameSite=Strict; HttpOnly\r\n").arg(m_authToken).toUtf8());
    }

    // CORS Estricto: Solo permitir localhost o nuestro propio host/puerto
    if (!origin.isEmpty()) {
        if (origin.contains(QStringLiteral("localhost")) || origin.contains(QStringLiteral("127.0.0.1")) || origin.contains(QString::number(m_port))) {
            header.append(QStringLiteral("Access-Control-Allow-Origin: %1\r\n").arg(origin).toUtf8());
            header.append("Access-Control-Allow-Credentials: true\r\n");
            header.append("Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n");
            header.append("Access-Control-Allow-Headers: Content-Type, X-Deck-Token, Authorization\r\n");
        }
    }

    header.append("Connection: close\r\n\r\n");

    socket->write(header);
    if (!body.isEmpty()) {
        socket->write(body);
    }
    socket->flush();
    socket->disconnectFromHost();
}

void DeckServer::broadcastTelemetry(int cpuPercent, int ramPercent, int tempC)
{
    m_lastCpu = cpuPercent;
    m_lastRam = ramPercent;
    m_lastTemp = tempC;

    if (m_sseClients.isEmpty()) {
        return;
    }

    QJsonObject obj;
    obj[QStringLiteral("type")] = QStringLiteral("telemetry");
    obj[QStringLiteral("cpu")] = cpuPercent;
    obj[QStringLiteral("ram")] = ramPercent;
    obj[QStringLiteral("temp")] = tempC;

    QByteArray msg = "data: " + QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n\n";

    for (int i = m_sseClients.size() - 1; i >= 0; --i) {
        QTcpSocket *sock = m_sseClients[i];
        if (sock && sock->state() == QAbstractSocket::ConnectedState) {
            sock->write(msg);
            sock->flush();
        } else {
            m_sseClients.removeAt(i);
        }
    }
}

void DeckServer::broadcastContext(const QString &appName, const QString &windowTitle, const QJsonArray &buttons)
{
    m_lastAppName = appName;
    m_lastWindowTitle = windowTitle;
    m_lastButtons = buttons;

    if (m_sseClients.isEmpty()) {
        return;
    }

    QJsonObject obj;
    obj[QStringLiteral("type")] = QStringLiteral("context");
    obj[QStringLiteral("app")] = appName;
    obj[QStringLiteral("title")] = windowTitle;
    obj[QStringLiteral("buttons")] = buttons;

    QByteArray msg = "data: " + QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n\n";

    for (int i = m_sseClients.size() - 1; i >= 0; --i) {
        QTcpSocket *sock = m_sseClients[i];
        if (sock && sock->state() == QAbstractSocket::ConnectedState) {
            sock->write(msg);
            sock->flush();
        } else {
            m_sseClients.removeAt(i);
        }
    }
}

QByteArray DeckServer::getIndexHtml() const
{
    return R"rawhtml(<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<meta name="theme-color" content="#0a0a10">
<title>Plasma Deck</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; user-select: none; -webkit-user-select: none; -webkit-tap-highlight-color: transparent; }
  body {
    background: #0a0a10;
    color: #f0f0f5;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
    min-height: 100vh;
    display: flex;
    flex-direction: column;
    padding: max(12px, env(safe-area-inset-top)) max(12px, env(safe-area-inset-right)) max(12px, env(safe-area-inset-bottom)) max(12px, env(safe-area-inset-left));
    overflow-x: hidden;
  }
  .hud {
    background: rgba(26, 26, 38, 0.7);
    backdrop-filter: blur(20px);
    -webkit-backdrop-filter: blur(20px);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 16px;
    padding: 10px 16px;
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 14px;
    box-shadow: 0 4px 20px rgba(0, 0, 0, 0.4);
  }
  .telemetry-item { display: flex; align-items: center; gap: 8px; }
  .telemetry-val { font-weight: 700; font-size: 15px; letter-spacing: -0.3px; }
  .telemetry-label { font-size: 11px; text-transform: uppercase; color: #8e8e9f; font-weight: 600; }
  .gauge-cpu { color: #38bdf8; }
  .gauge-ram { color: #a78bfa; }
  .gauge-temp { color: #f87171; }
  .app-badge {
    background: rgba(56, 189, 248, 0.12);
    border: 1px solid rgba(56, 189, 248, 0.3);
    color: #38bdf8;
    padding: 4px 10px;
    border-radius: 20px;
    font-size: 12px;
    font-weight: 600;
    display: flex;
    align-items: center;
    gap: 6px;
    max-width: 160px;
    overflow: hidden;
    white-space: nowrap;
    text-overflow: ellipsis;
  }
  .pulse-dot { width: 7px; height: 7px; border-radius: 50%; background: #38bdf8; box-shadow: 0 0 8px #38bdf8; animation: pulse 2s infinite; }
  @keyframes pulse { 0%, 100% { opacity: 1; transform: scale(1); } 50% { opacity: 0.4; transform: scale(0.85); } }

  /* RADAR CARD */
  .radar-card {
    background: rgba(22, 22, 34, 0.78);
    backdrop-filter: blur(20px);
    -webkit-backdrop-filter: blur(20px);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 16px;
    padding: 10px 14px;
    margin-bottom: 14px;
    box-shadow: 0 4px 20px rgba(0, 0, 0, 0.35);
    transition: all 0.2s ease;
  }
  .radar-card.active {
    border-color: rgba(56, 189, 248, 0.4);
    box-shadow: 0 0 20px rgba(56, 189, 248, 0.15);
  }
  .radar-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
  }
  .radar-info { display: flex; flex-direction: column; gap: 2px; }
  .radar-status-line { display: flex; align-items: center; gap: 8px; }
  .radar-dot {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background: #64748b;
    transition: all 0.2s ease;
  }
  .radar-dot.present { background: #4ade80; box-shadow: 0 0 8px #4ade80; }
  .radar-dot.absent { background: #facc15; box-shadow: 0 0 8px #facc15; }
  .radar-dot.locked { background: #f87171; box-shadow: 0 0 8px #f87171; }
  .radar-dot.gesture { background: #38bdf8; box-shadow: 0 0 12px #38bdf8; transform: scale(1.3); }

  .radar-title { font-size: 13px; font-weight: 700; color: #f0f0f5; letter-spacing: -0.2px; }
  .radar-sub { font-size: 10px; color: #8e8e9f; }

  /* SWITCH TOGGLE */
  .switch {
    position: relative;
    display: inline-block;
    width: 44px;
    height: 24px;
  }
  .switch input { opacity: 0; width: 0; height: 0; }
  .slider {
    position: absolute;
    cursor: pointer;
    top: 0; left: 0; right: 0; bottom: 0;
    background-color: #2e2e42;
    transition: .3s;
    border-radius: 24px;
    border: 1px solid rgba(255, 255, 255, 0.1);
  }
  .slider:before {
    position: absolute;
    content: "";
    height: 18px;
    width: 18px;
    left: 2px;
    bottom: 2px;
    background-color: #8e8e9f;
    transition: .3s;
    border-radius: 50%;
  }
  input:checked + .slider { background-color: rgba(56, 189, 248, 0.35); border-color: #38bdf8; }
  input:checked + .slider:before { transform: translateX(20px); background-color: #38bdf8; box-shadow: 0 0 8px #38bdf8; }

  .radar-details {
    display: flex;
    align-items: center;
    gap: 12px;
    margin-top: 10px;
    padding-top: 10px;
    border-top: 1px solid rgba(255, 255, 255, 0.06);
  }
  .radar-preview-wrap { position: relative; width: 80px; height: 60px; border-radius: 10px; overflow: hidden; border: 1px solid rgba(56, 189, 248, 0.3); flex-shrink: 0; }
  .radar-canvas-view { width: 100%; height: 100%; object-fit: cover; transform: scaleX(-1); }
  .radar-feed-status { display: flex; flex-direction: column; gap: 4px; font-size: 11px; color: #cbd5e1; }
  .feed-item b { color: #f0f0f5; }

  /* GESTURE TOAST NOTIFICATION */
  .gesture-toast {
    position: fixed;
    top: 40%;
    left: 50%;
    transform: translate(-50%, -50%) scale(0.8);
    background: rgba(18, 18, 28, 0.92);
    border: 1px solid rgba(56, 189, 248, 0.5);
    box-shadow: 0 10px 30px rgba(0, 0, 0, 0.7), 0 0 20px rgba(56, 189, 248, 0.3);
    backdrop-filter: blur(25px);
    -webkit-backdrop-filter: blur(25px);
    padding: 18px 26px;
    border-radius: 20px;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 8px;
    pointer-events: none;
    opacity: 0;
    transition: all 0.25s cubic-bezier(0.175, 0.885, 0.32, 1.275);
    z-index: 9999;
  }
  .gesture-toast.show {
    opacity: 1;
    transform: translate(-50%, -50%) scale(1);
  }
  .gesture-icon { font-size: 38px; filter: drop-shadow(0 2px 8px rgba(56,189,248,0.5)); }
  .gesture-label { font-size: 14px; font-weight: 700; color: #38bdf8; letter-spacing: -0.2px; }

  /* MODAL SECURE CONTEXT */
  .modal-overlay {
    position: fixed;
    top: 0; left: 0; right: 0; bottom: 0;
    background: rgba(0, 0, 0, 0.78);
    backdrop-filter: blur(12px);
    -webkit-backdrop-filter: blur(12px);
    display: flex;
    align-items: center;
    justify-content: center;
    z-index: 10000;
    padding: 20px;
  }
  .modal-content {
    background: #141420;
    border: 1px solid rgba(56, 189, 248, 0.4);
    box-shadow: 0 10px 40px rgba(0,0,0,0.8), 0 0 25px rgba(56, 189, 248, 0.2);
    border-radius: 18px;
    padding: 20px;
    max-width: 380px;
    width: 100%;
    text-align: center;
  }
  .modal-header {
    display: flex;
    align-items: center;
    gap: 10px;
    justify-content: center;
    color: #f0f0f5;
  }

  .deck-grid {
    flex: 1;
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    grid-template-rows: repeat(2, 1fr);
    gap: 12px;
    margin-bottom: 12px;
  }
  @media (orientation: portrait) {
    .deck-grid {
      grid-template-columns: repeat(2, 1fr);
      grid-template-rows: repeat(4, 1fr);
    }
  }

  .deck-key {
    background: linear-gradient(145deg, #181824 0%, #101018 100%);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 18px;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    padding: 12px 8px;
    cursor: pointer;
    position: relative;
    overflow: hidden;
    transition: all 0.12s cubic-bezier(0.4, 0, 0.2, 1);
    box-shadow: 0 6px 16px rgba(0, 0, 0, 0.35), inset 0 1px 1px rgba(255, 255, 255, 0.1);
  }
  .deck-key:active {
    transform: scale(0.94);
    background: linear-gradient(145deg, #222234 0%, #181824 100%);
    border-color: rgba(56, 189, 248, 0.5);
    box-shadow: 0 2px 8px rgba(0, 0, 0, 0.5), inset 0 0 16px rgba(56, 189, 248, 0.2);
  }
  .key-icon { font-size: 28px; margin-bottom: 6px; filter: drop-shadow(0 2px 4px rgba(0,0,0,0.4)); }
  .key-title { font-size: 13px; font-weight: 700; text-align: center; color: #f0f0f5; letter-spacing: -0.2px; line-height: 1.2; }
  .key-sub { font-size: 10px; color: #8e8e9f; margin-top: 3px; font-family: ui-monospace, monospace; }

  .profile-bar {
    background: rgba(20, 20, 30, 0.75);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 14px;
    padding: 6px;
    display: flex;
    gap: 6px;
    position: sticky;
    bottom: 0;
    backdrop-filter: blur(20px);
    -webkit-backdrop-filter: blur(20px);
    z-index: 100;
    margin-top: auto;
  }
  .profile-btn {
    flex: 1;
    background: transparent;
    border: none;
    color: #8e8e9f;
    font-size: 11px;
    font-weight: 600;
    padding: 8px 4px;
    border-radius: 10px;
    cursor: pointer;
    transition: all 0.15s ease;
  }
  .profile-btn.active {
    background: rgba(56, 189, 248, 0.18);
    color: #38bdf8;
  }
</style>
</head>
<body>

<div class="hud">
  <div class="telemetry-item">
    <div>
      <div class="telemetry-val gauge-cpu" id="cpuVal">--%</div>
      <div class="telemetry-label">CPU</div>
    </div>
  </div>
  <div class="telemetry-item">
    <div>
      <div class="telemetry-val gauge-temp" id="tempVal">--°C</div>
      <div class="telemetry-label">TEMP</div>
    </div>
  </div>
  <div class="telemetry-item">
    <div>
      <div class="telemetry-val gauge-ram" id="ramVal">--%</div>
      <div class="telemetry-label">RAM</div>
    </div>
  </div>
  <div class="app-badge" id="appBadge">
    <div class="pulse-dot"></div>
    <span id="appName">Plasma</span>
  </div>
</div>

<div class="radar-card" id="radarCard">
  <div class="radar-header">
    <div class="radar-info">
      <div class="radar-status-line">
        <span class="radar-dot" id="radarDot"></span>
        <span class="radar-title" id="radarTitle">Sensor Espacial: Inactivo</span>
      </div>
      <div class="radar-sub" id="radarSub">Poses anatómicas (👈 👉 👍 👎 ✋ ✊)</div>
    </div>
    <label class="switch">
      <input type="checkbox" id="radarSwitch" onchange="toggleOpticalRadar(this.checked)">
      <span class="slider"></span>
    </label>
  </div>
  <div class="radar-details" id="radarDetails" style="display:none;">
    <div class="radar-preview-wrap">
      <canvas id="radarPreview" width="96" height="72" class="radar-canvas-view"></canvas>
      <video id="radarVideo" playsinline muted autoplay style="display:none;"></video>
      <canvas id="radarWorkerCanvas" width="160" height="120" style="display:none;"></canvas>
    </div>
    <div class="radar-feed-status">
      <div class="feed-item" id="feedPresence">👁️ <b>Presencia:</b> Buscando usuario...</div>
      <div class="feed-item" id="feedGesture">🖐️ <b>Poses:</b> 👈 👉 Escritorios | 👍 👎 Vol | ✋ Media | ✊ Mic</div>
    </div>
  </div>
</div>

<div class="gesture-toast" id="gestureToast">
  <div class="gesture-icon" id="gestureToastIcon">➡️</div>
  <div class="gesture-label" id="gestureToastText">Siguiente Escritorio</div>
</div>

<div class="modal-overlay" id="secureNoticeModal" style="display:none;">
  <div class="modal-content">
    <div class="modal-header">
      <span style="font-size: 26px;">🔒</span>
      <h3>Autorizar Cámara Local</h3>
    </div>
    <p style="font-size: 13px; color: #cbd5e1; line-height: 1.5; margin: 12px 0;">
      Los navegadores Chromium (como Brave o Chrome) bloquean la cámara en redes locales HTTP salvo que se autorice la IP en los ajustes internos del navegador:
    </p>
    <div style="background: rgba(0,0,0,0.4); padding: 10px; border-radius: 10px; font-size: 12px; margin-bottom: 12px; text-align: left; line-height: 1.6;">
      <b>1.</b> Abre una nueva pestaña y escribe:<br>
      <code style="color: #38bdf8; font-weight: bold;">brave://flags</code> (o <code style="color: #38bdf8; font-weight: bold;">chrome://flags</code>)<br>
      <b>2.</b> Busca: <i>Insecure origins</i><br>
      <b>3.</b> Cambia a <b>Enabled</b> y pega en el cuadro:<br>
      <div style="display: flex; gap: 6px; margin-top: 6px;">
        <input type="text" id="originUrlInput" readonly style="flex: 1; background: #1a1a28; color: #38bdf8; border: 1px solid rgba(56,189,248,0.4); border-radius: 6px; padding: 4px 8px; font-size: 11px;">
        <button onclick="copyOriginUrl()" style="background: #38bdf8; color: #0a0a10; border: none; border-radius: 6px; padding: 4px 10px; font-weight: bold; font-size: 11px; cursor: pointer;">Copiar</button>
      </div>
      <b>4.</b> Pulsa el botón <b>Relaunch</b> (Reiniciar) al final de la pantalla.
    </div>
    <button onclick="document.getElementById('secureNoticeModal').style.display='none'" style="width: 100%; padding: 10px; background: rgba(255,255,255,0.1); border: 1px solid rgba(255,255,255,0.2); color: #fff; border-radius: 10px; font-weight: 600; cursor: pointer;">Entendido</button>
  </div>
</div>

<div class="deck-grid" id="deckGrid">
  <!-- Dynamic buttons inserted here -->
</div>

<div class="profile-bar">
  <button class="profile-btn active" id="btnAuto" onclick="setProfile('auto')">⚡ Auto</button>
  <button class="profile-btn" id="btnCode" onclick="setProfile('code')">💻 Código</button>
  <button class="profile-btn" id="btnFiles" onclick="setProfile('files')">📁 Archivos</button>
  <button class="profile-btn" id="btnWeb" onclick="setProfile('web')">🌐 Web</button>
  <button class="profile-btn" id="btnSystem" onclick="setProfile('system')">⚙️ Sistema</button>
</div>

<script>
  let activeProfile = 'auto';
  let currentContextApp = 'system';
  let sseButtons = [];

  const urlParams = new URLSearchParams(window.location.search);
  const deckToken = urlParams.get('token') || '';

  const defaultProfiles = {
    code: [
      { id: "build", icon: "🔨", title: "Compilar", sub: "CMake/Ninja", cmd: "build" },
      { id: "run", icon: "▶️", title: "Ejecutar", sub: "Run App", cmd: "run" },
      { id: "test", icon: "🧪", title: "Tests", sub: "ctest", cmd: "test" },
      { id: "git_commit", icon: "📦", title: "Git Commit", sub: "git commit", cmd: "git_commit" },
      { id: "clear", icon: "🧹", title: "Limpiar", sub: "clear", cmd: "clear" },
      { id: "term_split", icon: "🔀", title: "Dividir", sub: "Ctrl+Shift+E", cmd: "term_split" },
      { id: "search", icon: "🔍", title: "Buscar", sub: "Ctrl+Shift+F", cmd: "search" },
      { id: "save", icon: "💾", title: "Guardar", sub: "Ctrl+S", cmd: "save" }
    ],
    files: [
      { id: "new_folder", icon: "📁", title: "Nueva Carpeta", sub: "F10", cmd: "new_folder" },
      { id: "terminal_here", icon: "💻", title: "Terminal Aquí", sub: "F4", cmd: "terminal_here" },
      { id: "compress", icon: "🗜️", title: "Comprimir", sub: "Ark / Zip", cmd: "compress" },
      { id: "search_files", icon: "🔍", title: "Buscar", sub: "Ctrl+F", cmd: "search_files" },
      { id: "cut", icon: "✂️", title: "Cortar", sub: "Ctrl+X", cmd: "cut" },
      { id: "copy", icon: "📋", title: "Copiar", sub: "Ctrl+C", cmd: "copy" },
      { id: "paste", icon: "📌", title: "Pegar", sub: "Ctrl+V", cmd: "paste" },
      { id: "open_home", icon: "🏠", title: "Home", sub: "~/", cmd: "open_home" }
    ],
    web: [
      { id: "new_tab", icon: "➕", title: "Nueva Pestaña", sub: "Ctrl+T", cmd: "new_tab" },
      { id: "reload", icon: "🔄", title: "Recargar", sub: "Ctrl+R", cmd: "reload" },
      { id: "bookmark", icon: "⭐", title: "Marcador", sub: "Ctrl+D", cmd: "bookmark" },
      { id: "find_page", icon: "🔍", title: "Buscar", sub: "Ctrl+F", cmd: "find_page" },
      { id: "back", icon: "⬅️", title: "Atrás", sub: "Alt+Left", cmd: "back" },
      { id: "forward", icon: "➡️", title: "Adelante", sub: "Alt+Right", cmd: "forward" },
      { id: "downloads", icon: "⬇️", title: "Descargas", sub: "Ctrl+J", cmd: "downloads" },
      { id: "mute_tab", icon: "🔇", title: "Silenciar", sub: "Tab audio", cmd: "mute_tab" }
    ],
    system: [
      { id: "panic_shield", icon: "🚨", title: "Panic Shield", sub: "Privacidad", cmd: "panic_shield" },
      { id: "screenshot", icon: "📸", title: "Captura", sub: "Spectacle", cmd: "screenshot" },
      { id: "lock", icon: "🔒", title: "Bloquear", sub: "loginctl", cmd: "lock" },
      { id: "play_pause", icon: "⏯️", title: "Play/Pausa", sub: "Media", cmd: "play_pause" },
      { id: "next_track", icon: "⏭️", title: "Siguiente", sub: "Media", cmd: "next_track" },
      { id: "mute_sound", icon: "🔇", title: "Mute", sub: "PipeWire", cmd: "mute_sound" },
      { id: "grid_view", icon: "🔲", title: "Ventanas", sub: "Overview", cmd: "grid_view" },
      { id: "terminal", icon: "⚡", title: "Konsole", sub: "Terminal", cmd: "terminal" }
    ]
  };

  function renderGrid(buttons) {
    const grid = document.getElementById('deckGrid');
    grid.innerHTML = '';
    buttons.forEach(b => {
      const btn = document.createElement('div');
      btn.className = 'deck-key';
      if (b.id === 'panic_shield') {
        btn.style.border = '1px solid rgba(239, 68, 68, 0.6)';
        btn.style.background = 'linear-gradient(145deg, #2d141e 0%, #1a0b12 100%)';
        btn.style.boxShadow = '0 0 14px rgba(239, 68, 68, 0.25)';
      }
      btn.innerHTML = `
        <div class="key-icon">${b.icon || '⚡'}</div>
        <div class="key-title">${b.title}</div>
        <div class="key-sub">${b.sub || ''}</div>
      `;
      btn.addEventListener('click', () => triggerAction(b));
      grid.appendChild(btn);
    });
  }

  function triggerAction(btn) {
    if (navigator.vibrate) {
      navigator.vibrate(25);
    }
    const payload = Object.assign({}, btn, { token: deckToken });
    fetch('/api/action' + (deckToken ? ('?token=' + encodeURIComponent(deckToken)) : ''), {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'X-Deck-Token': deckToken
      },
      body: JSON.stringify(payload)
    }).catch(err => console.error(err));
  }

  function setProfile(prof) {
    activeProfile = prof;
    document.querySelectorAll('.profile-btn').forEach(b => b.classList.remove('active'));
    const btnId = 'btn' + prof.charAt(0).toUpperCase() + prof.slice(1);
    const elem = document.getElementById(btnId);
    if (elem) {
      elem.classList.add('active');
    }

    if (prof === 'auto') {
      applyContext(currentContextApp);
    } else {
      renderGrid(defaultProfiles[prof] || defaultProfiles.system);
    }
  }

  function applyContext(app) {
    currentContextApp = app;
    if (activeProfile !== 'auto') return;

    const lower = (app || '').toLowerCase();
    if (sseButtons && sseButtons.length > 0) {
      renderGrid(sseButtons);
      return;
    }

    if (lower.includes('code') || lower.includes('kate') || lower.includes('nvim') || lower.includes('idea') || lower.includes('konsole') || lower.includes('terminal')) {
      renderGrid(defaultProfiles.code);
    } else if (lower.includes('dolphin') || lower.includes('nautilus') || lower.includes('file') || lower.includes('ark')) {
      renderGrid(defaultProfiles.files);
    } else if (lower.includes('firefox') || lower.includes('chrome') || lower.includes('brave') || lower.includes('edge') || lower.includes('zen') || lower.includes('chromium') || lower.includes('browser')) {
      renderGrid(defaultProfiles.web);
    } else {
      renderGrid(defaultProfiles.system);
    }
  }

  // Server-Sent Events (SSE)
  function connectSSE() {
    const sseUrl = deckToken ? ('/api/events?token=' + encodeURIComponent(deckToken)) : '/api/events';
    const es = new EventSource(sseUrl);
    es.onmessage = (e) => {
      try {
        const data = JSON.parse(e.data);
        if (data.type === 'telemetry') {
          document.getElementById('cpuVal').textContent = data.cpu + '%';
          document.getElementById('ramVal').textContent = data.ram + '%';
          document.getElementById('tempVal').textContent = data.temp + '°C';
        } else if (data.type === 'context') {
          let name = data.app || 'Plasma';
          if (name.includes('.')) {
            name = name.split('.').pop();
          }
          if (name.length > 0) {
            name = name.charAt(0).toUpperCase() + name.slice(1);
          }
          document.getElementById('appName').textContent = name || 'Plasma';
          if (data.buttons && data.buttons.length > 0) {
            sseButtons = data.buttons;
          } else {
            sseButtons = [];
          }
          applyContext(data.app);
        }
      } catch (err) {
        console.error(err);
      }
    };
    es.onerror = () => {
      setTimeout(connectSSE, 3000);
    };
  }

  // ==========================================
  // SPATIAL RADAR & TOUCHLESS OPTICAL ENGINE
  // ==========================================
  let radarStream = null;
  let radarRunning = false;
  let radarAnimId = null;
  let prevGray = null;
  let gestureState = 'idle';
  let gestureStartTime = 0;
  let cooldownUntil = 0;
  let palmCoverStartTime = 0;
  let currentCandidatePose = 'none';
  let currentPoseHold = 0;
  let lastPresentTime = Date.now();
  let presenceState = 'present';
  let faceDetector = null;

  if ('FaceDetector' in window) {
    try {
      faceDetector = new FaceDetector({ fastMode: true, maxDetectedFaces: 1 });
    } catch(e) {
      faceDetector = null;
    }
  }

  function toggleOpticalRadar(enable) {
    if (enable) {
      startOpticalRadar();
    } else {
      stopOpticalRadar();
    }
  }

  function showSecureContextNotice() {
    const origin = window.location.protocol + '//' + window.location.host;
    const input = document.getElementById('originUrlInput');
    if (input) input.value = origin;
    const modal = document.getElementById('secureNoticeModal');
    if (modal) modal.style.display = 'flex';
  }

  function copyOriginUrl() {
    const input = document.getElementById('originUrlInput');
    if (!input) return;
    input.select();
    if (navigator.clipboard && navigator.clipboard.writeText) {
      navigator.clipboard.writeText(input.value);
    } else {
      document.execCommand('copy');
    }
    alert('URL copiada al portapapeles: ' + input.value);
  }

  async function startOpticalRadar() {
    try {
      if (!window.isSecureContext || !navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
        document.getElementById('radarSwitch').checked = false;
        showSecureContextNotice();
        return;
      }

      const constraints = {
        video: {
          facingMode: 'user',
          width: { ideal: 320 },
          height: { ideal: 240 }
        },
        audio: false
      };
      radarStream = await navigator.mediaDevices.getUserMedia(constraints);
      const video = document.getElementById('radarVideo');
      video.srcObject = radarStream;
      await video.play();

      radarRunning = true;
      document.getElementById('radarCard').classList.add('active');
      document.getElementById('radarDetails').style.display = 'flex';
      document.getElementById('radarTitle').textContent = 'Sensor Espacial: Activo';
      document.getElementById('radarSub').textContent = 'Monitorizando presencia y gestos en el aire';
      lastPresentTime = Date.now();
      presenceState = 'present';
      updateRadarPresenceUI('present', 'Atento / Presente');

      processRadarFrame();
    } catch (err) {
      console.error('Error starting camera radar:', err);
      document.getElementById('radarSwitch').checked = false;
      alert('No se pudo acceder a la cámara frontal para el Sensor Espacial: ' + err.message);
    }
  }

  function stopOpticalRadar() {
    radarRunning = false;
    if (radarAnimId) {
      cancelAnimationFrame(radarAnimId);
      radarAnimId = null;
    }
    if (radarStream) {
      radarStream.getTracks().forEach(t => t.stop());
      radarStream = null;
    }
    document.getElementById('radarCard').classList.remove('active');
    document.getElementById('radarDetails').style.display = 'none';
    document.getElementById('radarTitle').textContent = 'Sensor Espacial: Inactivo';
    document.getElementById('radarSub').textContent = 'Detección de presencia & Gestos en el aire';
    setRadarDot('idle');
  }

  function setRadarDot(state) {
    const dot = document.getElementById('radarDot');
    dot.className = 'radar-dot ' + state;
  }

  function updateRadarPresenceUI(state, text) {
    setRadarDot(state);
    const feed = document.getElementById('feedPresence');
    if (state === 'present') {
      feed.innerHTML = '👁️ <b>Presencia:</b> <span style="color:#4ade80">' + text + '</span>';
    } else if (state === 'absent') {
      feed.innerHTML = '🟡 <b>Presencia:</b> <span style="color:#facc15">' + text + '</span>';
    } else if (state === 'locked') {
      feed.innerHTML = '🔴 <b>Presencia:</b> <span style="color:#f87171">' + text + '</span>';
    }
  }

  function showGestureToast(icon, text) {
    const toast = document.getElementById('gestureToast');
    document.getElementById('gestureToastIcon').textContent = icon;
    document.getElementById('gestureToastText').textContent = text;
    toast.classList.add('show');
    if (navigator.vibrate) {
      navigator.vibrate(45);
    }
    setRadarDot('gesture');
    setTimeout(() => {
      toast.classList.remove('show');
      setRadarDot(presenceState);
    }, 900);
  }

  function sendSpatialEvent(type, val, durationMs) {
    const payload = {
      type: type,
      value: val,
      gesture: val,
      state: val,
      durationMs: durationMs || 0,
      token: deckToken
    };
    fetch('/api/spatial' + (deckToken ? ('?token=' + encodeURIComponent(deckToken)) : ''), {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'X-Deck-Token': deckToken
      },
      body: JSON.stringify(payload)
    }).catch(e => console.error(e));
  }

  let lastFrameTime = 0;
  const FRAME_INTERVAL = 50; // ~20 FPS

  async function processRadarFrame() {
    if (!radarRunning) return;

    const now = Date.now();
    if (now - lastFrameTime < FRAME_INTERVAL) {
      radarAnimId = requestAnimationFrame(processRadarFrame);
      return;
    }
    lastFrameTime = now;

    const video = document.getElementById('radarVideo');
    const workerCanvas = document.getElementById('radarWorkerCanvas');
    const previewCanvas = document.getElementById('radarPreview');

    if (!video.videoWidth || !video.videoHeight) {
      radarAnimId = requestAnimationFrame(processRadarFrame);
      return;
    }

    const w = 160;
    const h = 120;
    const ctx = workerCanvas.getContext('2d', { willReadFrequently: true });
    ctx.drawImage(video, 0, 0, w, h);

    const imgData = ctx.getImageData(0, 0, w, h);
    const data = imgData.data;

    // Mini preview render
    const prevCtx = previewCanvas.getContext('2d');
    prevCtx.drawImage(workerCanvas, 0, 0, previewCanvas.width, previewCanvas.height);

    // 1. Detección Óptica de Presencia / Rostro (YCbCr + Gradiente)
    let skinPixels = 0;
    let sumLuma = 0;
    let motionLeft = 0;
    let motionCenter = 0;
    let motionRight = 0;
    let motionTop = 0;
    let motionBottom = 0;

    const currentGray = new Uint8Array(w * h);

    for (let y = 0; y < h; y += 2) {
      for (let x = 0; x < w; x += 2) {
        const idx = (y * w + x) * 4;
        const r = data[idx];
        const g = data[idx + 1];
        const b = data[idx + 2];

        // Luma
        const luma = 0.299 * r + 0.587 * g + 0.114 * b;
        sumLuma += luma;
        currentGray[y * w + x] = luma;

        // YCbCr skin tone detection
        const cb = 128 - 0.168736 * r - 0.331264 * g + 0.500 * b;
        const cr = 128 + 0.500 * r - 0.418688 * g - 0.081312 * b;

        if (luma > 35 && luma < 240 && cb >= 77 && cb <= 127 && cr >= 133 && cr <= 173) {
          if (y < h * 0.8 && x > w * 0.15 && x < w * 0.85) {
            skinPixels++;
          }
        }

        // Motion differencing
        if (prevGray) {
          const diff = Math.abs(luma - prevGray[y * w + x]);
          if (diff > 22) {
            if (x < 54) motionLeft += diff;
            else if (x < 107) motionCenter += diff;
            else motionRight += diff;

            if (y < 60) motionTop += diff;
            else motionBottom += diff;
          }
        }
      }
    }

    prevGray = currentGray;
    const totalSampled = (w / 2) * (h / 2);
    const avgLuma = sumLuma / totalSampled;

    // FaceDetector API check if available
    let facePresent = skinPixels > 90;
    if (faceDetector && !facePresent) {
      try {
        const detected = await faceDetector.detect(video);
        if (detected.length > 0) facePresent = true;
      } catch(e) {}
    }

    // Evaluación de Presencia
    if (facePresent) {
      lastPresentTime = now;
      if (presenceState !== 'present') {
        presenceState = 'present';
        sendSpatialEvent('presence', 'present', 0);
        updateRadarPresenceUI('present', 'Atento / Presente');
      }
    } else {
      const absentDuration = now - lastPresentTime;
      if (absentDuration >= 60000) {
        if (presenceState !== 'locked') {
          presenceState = 'locked';
          sendSpatialEvent('presence', 'absent', absentDuration);
          updateRadarPresenceUI('locked', 'Ausente >60s (Sesión bloqueada)');
        }
      } else if (absentDuration >= 15000) {
        if (presenceState !== 'absent') {
          presenceState = 'absent';
          sendSpatialEvent('presence', 'absent', absentDuration);
          updateRadarPresenceUI('absent', 'Ausente >15s (Atenuando pantalla)');
        }
      }
    }

    // 2. Detección Óptica de Poses Anatómicas (Estrategia B: Inmune a ida y vuelta)
    // 👈 'pointing_left': Dedo índice a la izquierda -> Escritorio Anterior
    // 👉 'pointing_right': Dedo índice a la derecha -> Siguiente Escritorio
    // 👍 'thumb_up': Pulgar arriba -> Subir volumen
    // 👎 'thumb_down': Pulgar abajo -> Bajar volumen
    // ✋ 'palm_open': Palma abierta sostenida -> Play / Pausa Multimedia (YouTube, mpv)
    // ✊ 'fist_hold': Puño cerrado sostenido -> Silenciar Micrófono

    let minX = w, maxX = 0, minY = h, maxY = 0;
    let handPixels = 0;
    let handSumX = 0, handSumY = 0;

    const rowCounts = new Uint16Array(h);
    const colCounts = new Uint16Array(w);

    for (let y = 0; y < h; y += 2) {
      for (let x = 0; x < w; x += 2) {
        const idx = (y * w + x) * 4;
        const r = data[idx];
        const g = data[idx + 1];
        const b = data[idx + 2];
        const luma = 0.299 * r + 0.587 * g + 0.114 * b;
        const cb = 128 - 0.168736 * r - 0.331264 * g + 0.500 * b;
        const cr = 128 + 0.500 * r - 0.418688 * g - 0.081312 * b;

        if (luma > 35 && luma < 240 && cb >= 77 && cb <= 127 && cr >= 133 && cr <= 173) {
          handPixels++;
          handSumX += x;
          handSumY += y;
          if (x < minX) minX = x;
          if (x > maxX) maxX = x;
          if (y < minY) minY = y;
          if (y > maxY) maxY = y;
          rowCounts[y]++;
          colCounts[x]++;
        }
      }
    }

    let detectedPose = 'none';

    if (handPixels > 100) {
      const handW = maxX - minX;
      const handH = maxY - minY;
      const cx = handSumX / handPixels;
      const cy = handSumY / handPixels;
      const ar = handW / (handH || 1);

      // Dibujar caja de seguimiento en preview
      prevCtx.strokeStyle = '#38bdf8';
      prevCtx.lineWidth = 1.5;
      prevCtx.strokeRect((minX / w) * previewCanvas.width, (minY / h) * previewCanvas.height, (handW / w) * previewCanvas.width, (handH / h) * previewCanvas.height);

      let topCount = 0, bottomCount = 0;
      for (let y = minY; y < minY + handH * 0.35; y += 2) topCount += rowCounts[y];
      for (let y = maxY - handH * 0.35; y <= maxY; y += 2) bottomCount += rowCounts[y];

      let leftCount = 0, rightCount = 0;
      for (let x = minX; x < minX + handW * 0.35; x += 2) leftCount += colCounts[x];
      for (let x = maxX - handW * 0.35; x <= maxX; x += 2) rightCount += colCounts[x];

      if (handPixels > 450 && ar > 0.7 && ar < 1.4) {
        detectedPose = 'palm_open';
      } else if (handH > 32 && (cy - minY) > 1.25 * (maxY - cy) && topCount < bottomCount * 0.52) {
        detectedPose = 'thumb_up';
      } else if (handH > 32 && (maxY - cy) > 1.25 * (cy - minY) && bottomCount < topCount * 0.52) {
        detectedPose = 'thumb_down';
      } else if (handW > 35 && (cx - minX) > 1.25 * (maxX - cx) && leftCount < rightCount * 0.55) {
        detectedPose = 'pointing_left';
      } else if (handW > 35 && (maxX - cx) > 1.25 * (cx - minX) && rightCount < leftCount * 0.55) {
        detectedPose = 'pointing_right';
      } else if (handPixels >= 120 && handPixels <= 380 && ar > 0.75 && ar < 1.3) {
        detectedPose = 'fist_hold';
      }
    }

    // Feedback en vivo
    const feed = document.getElementById('feedGesture');
    const poseLabels = {
      pointing_left: '👈 Escritorio Izq',
      pointing_right: '👉 Escritorio Der',
      thumb_up: '👍 Volumen +',
      thumb_down: '👎 Volumen -',
      palm_open: '⏯️ Play / Pausa (Media)',
      fist_hold: '🎙️ Silenciar Micrófono'
    };

    if (detectedPose !== 'none') {
      const pct = Math.min(100, Math.round((currentPoseHold / 5) * 100));
      feed.innerHTML = '🖐️ <b>Pose:</b> <span style="color:#38bdf8">' + poseLabels[detectedPose] + ' (' + pct + '%)</span>';
    } else {
      feed.innerHTML = '🖐️ <b>Poses:</b> 👈 👉 Escritorios | 👍 👎 Vol | ✋ Media | ✊ Mic';
    }

    // Acumulador temporal para estabilidad (debe sostenerse 5 frames consecutivos ~250ms)
    if (now > cooldownUntil) {
      if (detectedPose !== 'none' && detectedPose === currentCandidatePose) {
        currentPoseHold++;
        if (currentPoseHold >= 5) {
          if (detectedPose === 'pointing_left') {
            showGestureToast('👈', 'Escritorio Anterior');
            sendSpatialEvent('gesture', 'pointing_left');
          } else if (detectedPose === 'pointing_right') {
            showGestureToast('👉', 'Siguiente Escritorio');
            sendSpatialEvent('gesture', 'pointing_right');
          } else if (detectedPose === 'thumb_up') {
            showGestureToast('👍', 'Volumen +');
            sendSpatialEvent('gesture', 'thumb_up');
          } else if (detectedPose === 'thumb_down') {
            showGestureToast('👎', 'Volumen -');
            sendSpatialEvent('gesture', 'thumb_down');
          } else if (detectedPose === 'palm_open') {
            showGestureToast('⏯️', 'Play / Pausa (YouTube / mpv)');
            sendSpatialEvent('gesture', 'palm_open');
          } else if (detectedPose === 'fist_hold') {
            showGestureToast('🎙️', 'Silenciar Micrófono');
            sendSpatialEvent('gesture', 'fist_hold');
          }
          cooldownUntil = now + 800;
          currentPoseHold = 0;
          currentCandidatePose = 'none';
        }
      } else {
        currentCandidatePose = detectedPose;
        currentPoseHold = 1;
      }
    }

    radarAnimId = requestAnimationFrame(processRadarFrame);
  }

  // Inicializar
  setProfile('auto');
  connectSSE();
</script>
</body>
</html>
)rawhtml";
}
