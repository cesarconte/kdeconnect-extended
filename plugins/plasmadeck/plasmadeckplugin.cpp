/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "plasmadeckplugin.h"

#include <KLocalizedString>
#include <KNotification>
#include <KPluginFactory>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QSettings>
#include <QTextStream>
#include <sys/sysinfo.h>

#include "core/device.h"
#include "core/networkpacket.h"

K_PLUGIN_CLASS_WITH_JSON(PlasmaDeckPlugin, "kdeconnect_plasmadeck.json")

#define PACKET_TYPE_SHARE_REQUEST QStringLiteral("kdeconnect.share.request")

PlasmaDeckPlugin::PlasmaDeckPlugin(QObject *parent, const QVariantList &args)
    : KdeConnectPlugin(parent, args)
    , m_server(1739, this)
{
    m_hostIp = findBestHostIp();

    connect(&m_server, &DeckServer::actionTriggered, this, &PlasmaDeckPlugin::handleAction);

    connect(&m_telemetryTimer, &QTimer::timeout, this, &PlasmaDeckPlugin::updateTelemetry);
    connect(&m_windowPollTimer, &QTimer::timeout, this, &PlasmaDeckPlugin::checkActiveWindow);

    m_server.start();

    // Iniciar muestreo de CPU y telemetría
    readCpuStats(m_prevWorkTime, m_prevTotalTime);
    updateTelemetry();

    m_telemetryTimer.start(1500);
    m_windowPollTimer.start(1000);
}

PlasmaDeckPlugin::~PlasmaDeckPlugin()
{
    m_server.stop();
}

void PlasmaDeckPlugin::connected()
{
    registerRunCommand();
}

QString PlasmaDeckPlugin::dbusPath() const
{
    return QLatin1String("/modules/kdeconnect/devices/%1/plasmadeck").arg(device()->id());
}

bool PlasmaDeckPlugin::isServerRunning() const
{
    return true;
}

QString PlasmaDeckPlugin::deckUrl() const
{
    return QStringLiteral("http://%1:1739/").arg(m_hostIp);
}

QString PlasmaDeckPlugin::activeApp() const
{
    return m_activeApp;
}

void PlasmaDeckPlugin::openPlasmaDeckOnPhone()
{
    const QString url = deckUrl();

    // Enviar URL al teléfono mediante kdeconnect.share.request
    NetworkPacket np(PACKET_TYPE_SHARE_REQUEST);
    np.set(QStringLiteral("url"), url);
    sendPacket(np);

    auto *notif = new KNotification(QStringLiteral("notification"), KNotification::CloseOnTimeout);
    notif->setIconName(QStringLiteral("input-gaming"));
    notif->setComponentName(QStringLiteral("kdeconnect"));
    notif->setTitle(i18n("Plasma Deck"));
    notif->setText(i18n("Abriendo Plasma Deck en %1 (%2)", device()->name(), url));
    notif->sendEvent();
}

void PlasmaDeckPlugin::triggerAction(const QString &actionId)
{
    handleAction(actionId, QString(), QString());
}

void PlasmaDeckPlugin::onActiveWindowChanged(const QString &appClass, const QString &windowTitle)
{
    if (appClass.isEmpty()) {
        return;
    }

    m_activeApp = appClass;
    m_windowTitle = windowTitle;

    m_server.broadcastContext(m_activeApp, m_windowTitle, QJsonArray());
    Q_EMIT activeAppChanged(m_activeApp);
}

void PlasmaDeckPlugin::updateTelemetry()
{
    int cpu = readCpuPercent();
    int ram = readRamPercent();
    int temp = readTempC();

    m_server.broadcastTelemetry(cpu, ram, temp);
}

void PlasmaDeckPlugin::checkActiveWindow()
{
    // Consultar KWin o servicios activos para deducir ventana en primer plano
    QDBusInterface kwinIface(QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"), QDBusConnection::sessionBus());
    if (kwinIface.isValid()) {
        // En KDE 6 Wayland, comprobar si KWin tiene la propiedad activa
    }
}

bool PlasmaDeckPlugin::triggerDolphinAction(const QString &actionName)
{
    if (!QDBusConnection::sessionBus().interface()) {
        return false;
    }

    const auto serviceNames = QDBusConnection::sessionBus().interface()->registeredServiceNames().value();
    bool triggered = false;
    for (const auto &s : serviceNames) {
        if (s.startsWith(QLatin1String("org.kde.dolphin-"))) {
            QDBusInterface dolphinIface(s,
                                        QStringLiteral("/dolphin/Dolphin_1/actions/%1").arg(actionName),
                                        QStringLiteral("org.qtproject.Qt.QAction"),
                                        QDBusConnection::sessionBus());
            if (dolphinIface.isValid()) {
                dolphinIface.call(QDBus::NoBlock, QStringLiteral("trigger"));
                triggered = true;
            }
        }
    }
    return triggered;
}

void PlasmaDeckPlugin::sendKeyShortcut(const QString &key, int specialKey, bool ctrl, bool alt, bool shift, bool super)
{
    auto *mousepad = device()->plugin(QStringLiteral("kdeconnect_mousepad"));
    if (!mousepad) {
        mousepad = device()->plugin(QStringLiteral("mousepad"));
    }
    if (mousepad) {
        NetworkPacket np(QStringLiteral("kdeconnect.mousepad.request"));
        if (!key.isEmpty()) {
            np.set(QStringLiteral("key"), key);
        }
        if (specialKey > 0) {
            np.set(QStringLiteral("specialKey"), specialKey);
        }
        if (ctrl) {
            np.set(QStringLiteral("ctrl"), true);
        }
        if (alt) {
            np.set(QStringLiteral("alt"), true);
        }
        if (shift) {
            np.set(QStringLiteral("shift"), true);
        }
        if (super) {
            np.set(QStringLiteral("super"), true);
        }

        mousepad->receivePacket(np);
    }
}

void PlasmaDeckPlugin::handleAction(const QString &actionId, const QString &cmd, const QString &shortcut)
{
    Q_UNUSED(cmd)
    Q_UNUSED(shortcut)

    // SYSTEM ACTIONS
    if (actionId == QLatin1String("screenshot")) {
        QProcess::startDetached(QStringLiteral("spectacle"), {QStringLiteral("-r")});
    } else if (actionId == QLatin1String("lock")) {
        QProcess::startDetached(QStringLiteral("loginctl"), {QStringLiteral("lock-session")});
    } else if (actionId == QLatin1String("night_color")) {
        QDBusInterface iface(QStringLiteral("org.kde.kglobalaccel"),
                             QStringLiteral("/component/kwin"),
                             QStringLiteral("org.kde.kglobalaccel.Component"),
                             QDBusConnection::sessionBus());
        iface.call(QDBus::NoBlock, QStringLiteral("invokeShortcut"), QStringLiteral("Toggle Night Color"));
    } else if (actionId == QLatin1String("grid_view")) {
        QDBusInterface iface(QStringLiteral("org.kde.kglobalaccel"),
                             QStringLiteral("/component/kwin"),
                             QStringLiteral("org.kde.kglobalaccel.Component"),
                             QDBusConnection::sessionBus());
        iface.call(QDBus::NoBlock, QStringLiteral("invokeShortcut"), QStringLiteral("Grid View"));
    } else if (actionId == QLatin1String("terminal")) {
        QProcess::startDetached(QStringLiteral("konsole"), {});
    } else if (actionId == QLatin1String("play_pause")) {
        QDBusInterface iface(QStringLiteral("org.kde.kglobalaccel"),
                             QStringLiteral("/component/mediacontrol"),
                             QStringLiteral("org.kde.kglobalaccel.Component"),
                             QDBusConnection::sessionBus());
        iface.call(QDBus::NoBlock, QStringLiteral("invokeShortcut"), QStringLiteral("playpausemedia"));
    } else if (actionId == QLatin1String("next_track")) {
        QDBusInterface iface(QStringLiteral("org.kde.kglobalaccel"),
                             QStringLiteral("/component/mediacontrol"),
                             QStringLiteral("org.kde.kglobalaccel.Component"),
                             QDBusConnection::sessionBus());
        iface.call(QDBus::NoBlock, QStringLiteral("invokeShortcut"), QStringLiteral("nextmedia"));
    } else if (actionId == QLatin1String("mute_sound")) {
        QDBusInterface iface(QStringLiteral("org.kde.kglobalaccel"),
                             QStringLiteral("/component/kmix"),
                             QStringLiteral("org.kde.kglobalaccel.Component"),
                             QDBusConnection::sessionBus());
        iface.call(QDBus::NoBlock, QStringLiteral("invokeShortcut"), QStringLiteral("mute"));

        // FILES ACTIONS
    } else if (actionId == QLatin1String("open_home")) {
        QProcess::startDetached(QStringLiteral("dolphin"), {QDir::homePath()});
    } else if (actionId == QLatin1String("new_folder")) {
        bool triggered = triggerDolphinAction(QStringLiteral("create_dir"));
        sendKeyShortcut(QString(), 30, false, false, false, false); // F10
        if (!triggered) {
            QProcess::startDetached(QStringLiteral("dolphin"), {});
        }
    } else if (actionId == QLatin1String("terminal_here")) {
        bool triggered = triggerDolphinAction(QStringLiteral("open_terminal_here")) || triggerDolphinAction(QStringLiteral("open_terminal"));
        sendKeyShortcut(QString(), 24, false, false, false, false); // F4
        if (!triggered) {
            QProcess::startDetached(QStringLiteral("konsole"), {});
        }
    } else if (actionId == QLatin1String("compress")) {
        QProcess::startDetached(QStringLiteral("ark"), {});
    } else if (actionId == QLatin1String("search_files")) {
        triggerDolphinAction(QStringLiteral("toggle_search"));
        sendKeyShortcut(QStringLiteral("f"), 0, true, false, false, false); // Ctrl+F
    } else if (actionId == QLatin1String("cut")) {
        triggerDolphinAction(QStringLiteral("edit_cut"));
        sendKeyShortcut(QStringLiteral("x"), 0, true, false, false, false); // Ctrl+X
    } else if (actionId == QLatin1String("copy")) {
        triggerDolphinAction(QStringLiteral("edit_copy"));
        sendKeyShortcut(QStringLiteral("c"), 0, true, false, false, false); // Ctrl+C
    } else if (actionId == QLatin1String("paste")) {
        triggerDolphinAction(QStringLiteral("edit_paste"));
        sendKeyShortcut(QStringLiteral("v"), 0, true, false, false, false); // Ctrl+V

        // WEB ACTIONS
    } else if (actionId == QLatin1String("new_tab")) {
        sendKeyShortcut(QStringLiteral("t"), 0, true, false, false, false); // Ctrl+T
        QProcess::startDetached(QStringLiteral("xdg-open"), {QStringLiteral("chrome://newtab")});
    } else if (actionId == QLatin1String("reload")) {
        sendKeyShortcut(QStringLiteral("r"), 0, true, false, false, false); // Ctrl+R
        sendKeyShortcut(QString(), 25, false, false, false, false); // F5
    } else if (actionId == QLatin1String("bookmark")) {
        sendKeyShortcut(QStringLiteral("d"), 0, true, false, false, false); // Ctrl+D
    } else if (actionId == QLatin1String("find_page")) {
        sendKeyShortcut(QStringLiteral("f"), 0, true, false, false, false); // Ctrl+F
    } else if (actionId == QLatin1String("back")) {
        sendKeyShortcut(QString(), 4, false, true, false, false); // Alt+Left
    } else if (actionId == QLatin1String("forward")) {
        sendKeyShortcut(QString(), 6, false, true, false, false); // Alt+Right
    } else if (actionId == QLatin1String("downloads")) {
        sendKeyShortcut(QStringLiteral("j"), 0, true, false, false, false); // Ctrl+J
        QProcess::startDetached(QStringLiteral("xdg-open"), {QDir::homePath() + QStringLiteral("/Descargas")});
    } else if (actionId == QLatin1String("mute_tab")) {
        sendKeyShortcut(QStringLiteral("m"), 0, true, false, false, false); // Ctrl+M

        // CODE ACTIONS
    } else if (actionId == QLatin1String("build")) {
        sendKeyShortcut(QStringLiteral("b"), 0, true, false, false, false); // Ctrl+B
        sendKeyShortcut(QString(), 27, false, false, false, false); // F7
    } else if (actionId == QLatin1String("run")) {
        sendKeyShortcut(QString(), 25, false, false, false, false); // F5
    } else if (actionId == QLatin1String("test")) {
        QProcess::startDetached(QStringLiteral("konsole"), {QStringLiteral("-e"), QStringLiteral("ctest --output-on-failure")});
    } else if (actionId == QLatin1String("git_commit")) {
        sendKeyShortcut(QStringLiteral("k"), 0, true, false, false, false); // Ctrl+K
    } else if (actionId == QLatin1String("clear")) {
        sendKeyShortcut(QStringLiteral("l"), 0, true, false, false, false); // Ctrl+L
    } else if (actionId == QLatin1String("term_split")) {
        sendKeyShortcut(QStringLiteral("e"), 0, true, false, true, false); // Ctrl+Shift+E
    } else if (actionId == QLatin1String("search")) {
        sendKeyShortcut(QStringLiteral("f"), 0, true, false, true, false); // Ctrl+Shift+F
    } else if (actionId == QLatin1String("save")) {
        sendKeyShortcut(QStringLiteral("s"), 0, true, false, false, false); // Ctrl+S
    }
}

void PlasmaDeckPlugin::readCpuStats(unsigned long long &workTime, unsigned long long &totalTime)
{
    workTime = 0;
    totalTime = 0;

    QFile file(QStringLiteral("/proc/stat"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QTextStream stream(&file);
    QString line = stream.readLine();
    if (!line.startsWith(QLatin1String("cpu "))) {
        return;
    }

    const QStringList parts = line.simplified().split(QLatin1Char(' '));
    if (parts.size() < 5) {
        return;
    }

    unsigned long long user = parts[1].toULongLong();
    unsigned long long nice = parts[2].toULongLong();
    unsigned long long system = parts[3].toULongLong();
    unsigned long long idle = parts[4].toULongLong();
    unsigned long long iowait = (parts.size() > 5) ? parts[5].toULongLong() : 0;
    unsigned long long irq = (parts.size() > 6) ? parts[6].toULongLong() : 0;
    unsigned long long softirq = (parts.size() > 7) ? parts[7].toULongLong() : 0;

    workTime = user + nice + system + irq + softirq;
    totalTime = workTime + idle + iowait;
}

int PlasmaDeckPlugin::readCpuPercent()
{
    unsigned long long workTime = 0;
    unsigned long long totalTime = 0;
    readCpuStats(workTime, totalTime);

    if (m_prevTotalTime == 0 || totalTime <= m_prevTotalTime) {
        m_prevWorkTime = workTime;
        m_prevTotalTime = totalTime;
        return 0;
    }

    unsigned long long workDelta = workTime - m_prevWorkTime;
    unsigned long long totalDelta = totalTime - m_prevTotalTime;

    m_prevWorkTime = workTime;
    m_prevTotalTime = totalTime;

    if (totalDelta == 0) {
        return 0;
    }

    int percent = static_cast<int>((workDelta * 100) / totalDelta);
    return qBound(0, percent, 100);
}

int PlasmaDeckPlugin::readRamPercent()
{
    QFile file(QStringLiteral("/proc/meminfo"));
    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray content = file.readAll();
        unsigned long long totalKb = 0;
        unsigned long long availableKb = 0;

        const auto lines = content.split('\n');
        for (const auto &line : lines) {
            if (line.startsWith("MemTotal:")) {
                totalKb = line.simplified().split(' ').value(1).toULongLong();
            } else if (line.startsWith("MemAvailable:")) {
                availableKb = line.simplified().split(' ').value(1).toULongLong();
            }
            if (totalKb > 0 && availableKb > 0) {
                break;
            }
        }

        if (totalKb > 0) {
            int usedPercent = static_cast<int>(((totalKb - availableKb) * 100) / totalKb);
            return qBound(0, usedPercent, 100);
        }
    }

    // Fallback con syscall directa del kernel sysinfo(2)
    struct sysinfo info;
    if (sysinfo(&info) == 0 && info.totalram > 0) {
        unsigned long long total = info.totalram;
        unsigned long long free = info.freeram + info.bufferram;
        unsigned long long used = (total > free) ? (total - free) : 0;
        int percent = static_cast<int>((used * 100) / total);
        return qBound(0, percent, 100);
    }

    return 0;
}

int PlasmaDeckPlugin::readTempC()
{
    QFile file(QStringLiteral("/sys/class/thermal/thermal_zone0/temp"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 35; // Fallback razonable
    }

    QTextStream stream(&file);
    long tempMilli = stream.readLine().trimmed().toLong();
    if (tempMilli > 1000) {
        return static_cast<int>(tempMilli / 1000);
    }
    return static_cast<int>(tempMilli);
}

QString PlasmaDeckPlugin::findBestHostIp() const
{
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const auto &iface : interfaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp) || (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        const auto entries = iface.addressEntries();
        for (const auto &entry : entries) {
            QHostAddress addr = entry.ip();
            if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
                QString ip = addr.toString();
                if (ip.startsWith(QLatin1String("192.168."))) {
                    return ip;
                }
            }
        }
    }

    return QStringLiteral("127.0.0.1");
}

void PlasmaDeckPlugin::registerRunCommand()
{
    // Registrar orden en kdeconnect_runcommand para que el usuario pueda abrir Plasma Deck desde la app
    const QString configPath =
        QStringLiteral("%1/%2/kdeconnect_runcommand/config").arg(QDir::homePath() + QStringLiteral("/.config/kdeconnect")).arg(device()->id());

    QSettings s(configPath, QSettings::IniFormat);
    QJsonObject json = QJsonDocument::fromJson(s.value(QStringLiteral("commands")).toByteArray()).object();

    QJsonObject deckEntry;
    deckEntry[QStringLiteral("name")] = QString::fromUtf8("🎛 Abrir Plasma Deck");
    deckEntry[QStringLiteral("command")] =
        QStringLiteral(
            "/usr/bin/qdbus6 org.kde.kdeconnect /modules/kdeconnect/devices/%1/plasmadeck org.kde.kdeconnect.device.plasmadeck.openPlasmaDeckOnPhone")
            .arg(device()->id());

    json[QStringLiteral("plasma_deck_open")] = deckEntry;

    QJsonDocument doc(json);
    s.setValue(QStringLiteral("commands"), doc.toJson(QJsonDocument::Compact));
    s.sync();
}

#include "moc_plasmadeckplugin.cpp"
#include "plasmadeckplugin.moc"
