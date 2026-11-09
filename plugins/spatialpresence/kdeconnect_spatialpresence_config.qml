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
        presenceDimming.checked = config.getBool("presenceDimmingEnabled", true)
        presenceLock.checked = config.getBool("presenceLockEnabled", true)
        waveGestures.checked = config.getBool("waveGesturesEnabled", true)
        micHoldGesture.checked = config.getBool("micHoldGestureEnabled", true)
        volumeGestures.checked = config.getBool("volumeGesturesEnabled", true)
        dimTimeout.value = config.getInt("dimTimeoutSeconds", 15)
        lockTimeout.value = config.getInt("lockTimeoutSeconds", 60)
        dimBrightness.value = config.getInt("dimBrightnessPercent", 20)
    }

    Kirigami.FormLayout {
        KdeConnectPluginConfig {
            id: config
            deviceId: device
            pluginName: "kdeconnect_spatialpresence"
        }

        QQC2.Label {
            text: i18nd("kdeconnect-plugins", "Attention & Presence Tracking:")
            font.bold: true
        }

        QQC2.CheckBox {
            id: presenceDimming
            text: i18nd("kdeconnect-plugins", "Dim screen when looking away or absent (Silent Dimming)")
            onClicked: config.set("presenceDimmingEnabled", checked)
        }

        QQC2.SpinBox {
            id: dimTimeout
            Kirigami.FormData.label: i18nd("kdeconnect-plugins", "Absence before dimming (seconds):")
            from: 5
            to: 120
            stepSize: 5
            onValueModified: config.set("dimTimeoutSeconds", value)
        }

        QQC2.SpinBox {
            id: dimBrightness
            Kirigami.FormData.label: i18nd("kdeconnect-plugins", "Dim brightness level (%):")
            from: 5
            to: 50
            stepSize: 5
            onValueModified: config.set("dimBrightnessPercent", value)
        }

        QQC2.CheckBox {
            id: presenceLock
            text: i18nd("kdeconnect-plugins", "Lock session upon prolonged absence")
            onClicked: config.set("presenceLockEnabled", checked)
        }

        QQC2.SpinBox {
            id: lockTimeout
            Kirigami.FormData.label: i18nd("kdeconnect-plugins", "Absence before lock (seconds):")
            from: 30
            to: 600
            stepSize: 15
            onValueModified: config.set("lockTimeoutSeconds", value)
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
        }

        QQC2.Label {
            text: i18nd("kdeconnect-plugins", "Touchless Air Gestures:")
            font.bold: true
        }

        QQC2.CheckBox {
            id: waveGestures
            text: i18nd("kdeconnect-plugins", "Wave left/right to switch virtual desktops in KWin")
            onClicked: config.set("waveGesturesEnabled", checked)
        }

        QQC2.CheckBox {
            id: micHoldGesture
            text: i18nd("kdeconnect-plugins", "Hold palm over camera to mute/unmute microphone")
            onClicked: config.set("micHoldGestureEnabled", checked)
        }

        QQC2.CheckBox {
            id: volumeGestures
            text: i18nd("kdeconnect-plugins", "Wave up/down to increase/decrease volume")
            onClicked: config.set("volumeGesturesEnabled", checked)
        }
    }
}
