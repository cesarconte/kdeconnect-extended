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

  // Inicializar
  setProfile('auto');
  connectSSE();
</script>
</body>
</html>
)rawhtml";
}
