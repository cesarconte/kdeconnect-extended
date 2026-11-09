/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>

#include "deckserver.h"
#include <core/kdeconnectplugin.h>

class PlasmaDeckPlugin : public KdeConnectPlugin
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeconnect.device.plasmadeck")
    Q_PROPERTY(bool isServerRunning READ isServerRunning NOTIFY serverStateChanged)
    Q_PROPERTY(QString deckUrl READ deckUrl NOTIFY deckUrlChanged)
    Q_PROPERTY(QString activeApp READ activeApp NOTIFY activeAppChanged)

public:
    explicit PlasmaDeckPlugin(QObject *parent, const QVariantList &args);
    ~PlasmaDeckPlugin() override;

    void connected() override;
    QString dbusPath() const override;

    bool isServerRunning() const;
    QString deckUrl() const;
    QString activeApp() const;

public Q_SLOTS:
    Q_SCRIPTABLE void openPlasmaDeckOnPhone();
    Q_SCRIPTABLE void triggerAction(const QString &actionId);
    Q_SCRIPTABLE void onActiveWindowChanged(const QString &appClass, const QString &windowTitle);

Q_SIGNALS:
    Q_SCRIPTABLE void serverStateChanged(bool running);
    Q_SCRIPTABLE void deckUrlChanged(const QString &url);
    Q_SCRIPTABLE void activeAppChanged(const QString &appName);

private Q_SLOTS:
    void updateTelemetry();
    void checkActiveWindow();
    void handleAction(const QString &actionId, const QString &cmd, const QString &shortcut);

private:
    void readCpuStats(unsigned long long &workTime, unsigned long long &totalTime);
    int readCpuPercent();
    int readRamPercent();
    int readTempC();
    QString findBestHostIp() const;
    void registerRunCommand();
    bool triggerDolphinAction(const QString &actionName);
    void sendKeyShortcut(const QString &key, int specialKey = 0, bool ctrl = false, bool alt = false, bool shift = false, bool super = false);

    DeckServer m_server;
    QTimer m_telemetryTimer;
    QTimer m_windowPollTimer;

    unsigned long long m_prevWorkTime = 0;
    unsigned long long m_prevTotalTime = 0;

    QString m_activeApp = QStringLiteral("Plasma");
    QString m_windowTitle;
    QString m_hostIp;
};
