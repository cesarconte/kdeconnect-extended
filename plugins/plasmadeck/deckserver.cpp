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

QString DeckServer::serverUrl(const QString &hostIp) const
{
    return QStringLiteral("http://%1:%2/").arg(hostIp).arg(m_port);
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

    const QString req = QString::fromUtf8(data);
    const QString firstLine = req.section(QLatin1Char('\n'), 0, 0).trimmed();
    const QStringList parts = firstLine.split(QLatin1Char(' '));
    if (parts.size() < 2) {
        return;
    }

    const QString method = parts[0];
    const QString path = parts[1];

    if (method == QLatin1String("OPTIONS")) {
        QByteArray resp =
            "HTTP/1.1 204 No Content\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type\r\n"
            "Connection: close\r\n\r\n";
        socket->write(resp);
        socket->flush();
        socket->disconnectFromHost();
        return;
    }

    if (method == QLatin1String("GET") && (path == QLatin1String("/") || path == QLatin1String("/index.html"))) {
        sendResponse(socket, 200, QStringLiteral("text/html; charset=utf-8"), getIndexHtml());
        return;
    }

    if (method == QLatin1String("GET") && path == QLatin1String("/api/events")) {
        // Registrar cliente Server-Sent Events (SSE)
        QByteArray sseHeader =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: keep-alive\r\n"
            "Access-Control-Allow-Origin: *\r\n\r\n";
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

    if (method == QLatin1String("POST") && path == QLatin1String("/api/action")) {
        int bodyIdx = data.indexOf("\r\n\r\n");
        if (bodyIdx == -1) {
            bodyIdx = data.indexOf("\n\n");
        }

        QByteArray body = (bodyIdx != -1) ? data.mid(bodyIdx + 4) : QByteArray();
        QJsonDocument doc = QJsonDocument::fromJson(body);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            const QString id = obj.value(QStringLiteral("id")).toString();
            const QString cmd = obj.value(QStringLiteral("command")).toString();
            const QString shortcut = obj.value(QStringLiteral("shortcut")).toString();
            Q_EMIT actionTriggered(id, cmd, shortcut);
        }

        QByteArray resp = "{\"status\":\"ok\"}";
        sendResponse(socket, 200, QStringLiteral("application/json"), resp);
        return;
    }

    // 404 por defecto
    sendResponse(socket, 404, QStringLiteral("text/plain"), "Not Found");
}

void DeckServer::sendResponse(QTcpSocket *socket, int statusCode, const QString &contentType, const QByteArray &body)
{
    QString statusText = (statusCode == 200) ? QStringLiteral("OK") : QStringLiteral("Not Found");
    QByteArray header;
    header.append(QStringLiteral("HTTP/1.1 %1 %2\r\n").arg(statusCode).arg(statusText).toUtf8());
    header.append(QStringLiteral("Content-Type: %1\r\n").arg(contentType).toUtf8());
    header.append(QStringLiteral("Content-Length: %1\r\n").arg(body.size()).toUtf8());
    header.append("Access-Control-Allow-Origin: *\r\n");
    header.append("Connection: close\r\n\r\n");

    socket->write(header);
    socket->write(body);
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
    background: rgba(20, 20, 30, 0.6);
    border: 1px solid rgba(255, 255, 255, 0.06);
    border-radius: 14px;
    padding: 6px;
    display: flex;
    gap: 6px;
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
  <button class="profile-btn" id="btnSys" onclick="setProfile('system')">⚙️ Sistema</button>
</div>

<script>
  let activeProfile = 'auto';
  let currentContextApp = 'system';
  let sseButtons = [];

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
      { id: "screenshot", icon: "📸", title: "Captura", sub: "Spectacle", cmd: "screenshot" },
      { id: "lock", icon: "🔒", title: "Bloquear", sub: "loginctl", cmd: "lock" },
      { id: "play_pause", icon: "⏯️", title: "Play/Pausa", sub: "Media", cmd: "play_pause" },
      { id: "next_track", icon: "⏭️", title: "Siguiente", sub: "Media", cmd: "next_track" },
      { id: "mute_sound", icon: "🔇", title: "Mute", sub: "PipeWire", cmd: "mute_sound" },
      { id: "night_color", icon: "🌙", title: "Noche", sub: "KWin Color", cmd: "night_color" },
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
    fetch('/api/action', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(btn)
    }).catch(err => console.error(err));
  }

  function setProfile(prof) {
    activeProfile = prof;
    document.querySelectorAll('.profile-btn').forEach(b => b.classList.remove('active'));
    document.getElementById('btn' + prof.charAt(0).toUpperCase() + prof.slice(1)).classList.add('active');

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

    if (lower.includes('code') || lower.includes('kate') || lower.includes('nvim') || lower.includes('idea')) {
      renderGrid(defaultProfiles.code);
    } else if (lower.includes('dolphin') || lower.includes('nautilus') || lower.includes('file')) {
      renderGrid(defaultProfiles.files);
    } else if (lower.includes('firefox') || lower.includes('chrome') || lower.includes('brave') || lower.includes('edge')) {
      renderGrid(defaultProfiles.web);
    } else {
      renderGrid(defaultProfiles.system);
    }
  }

  // Server-Sent Events (SSE)
  function connectSSE() {
    const es = new EventSource('/api/events');
    es.onmessage = (e) => {
      try {
        const data = JSON.parse(e.data);
        if (data.type === 'telemetry') {
          document.getElementById('cpuVal').textContent = data.cpu + '%';
          document.getElementById('ramVal').textContent = data.ram + '%';
          document.getElementById('tempVal').textContent = data.temp + '°C';
        } else if (data.type === 'context') {
          document.getElementById('appName').textContent = data.app || 'Plasma';
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
