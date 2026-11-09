/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami
import org.kde.kdeconnect 1.0

Kirigami.ScrollablePage {
    id: root

    property string device

    Component.onCompleted: {
        hideDesktop.checked = config.getBool("hideDesktop", true)
        muteAudio.checked = config.getBool("muteAudio", true)
        pauseMedia.checked = config.getBool("pauseMedia", true)
        inhibitNotifications.checked = config.getBool("inhibitNotifications", true)
        lockSession.checked = config.getBool("lockSession", false)
    }

    Kirigami.FormLayout {
        KdeConnectPluginConfig {
            id: config
            deviceId: device
            pluginName: "kdeconnect_panicshield"
        }

        QQC2.Label {
            text: i18nd("kdeconnect-plugins", "Emergency Privacy Actions:")
            font.bold: true
        }

        QQC2.CheckBox {
            id: hideDesktop
            text: i18nd("kdeconnect-plugins", "Hide all open windows (Show Desktop / Boss Key)")
            onClicked: config.set("hideDesktop", checked)
        }

        QQC2.CheckBox {
            id: muteAudio
            text: i18nd("kdeconnect-plugins", "Instantly mute system audio (PipeWire)")
            onClicked: config.set("muteAudio", checked)
        }

        QQC2.CheckBox {
            id: pauseMedia
            text: i18nd("kdeconnect-plugins", "Pause all active media playback (MPRIS)")
            onClicked: config.set("pauseMedia", checked)
        }

        QQC2.CheckBox {
            id: inhibitNotifications
            text: i18nd("kdeconnect-plugins", "Suppress popup notifications (Do Not Disturb)")
            onClicked: config.set("inhibitNotifications", checked)
        }

        QQC2.CheckBox {
            id: lockSession
            text: i18nd("kdeconnect-plugins", "Lock screen session immediately upon panic activation")
            onClicked: config.set("lockSession", checked)
        }
    }
}
