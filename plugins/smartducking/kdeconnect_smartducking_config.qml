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
    property string device

    Kirigami.FormLayout {

        KdeConnectPluginConfig {
            id: config
            deviceId: device
            pluginName: "kdeconnect_smartducking"
        }

        Component.onCompleted: {
            duckingSlider.value = config.getInt("duckingLevel", 15)
            rampSlider.value = config.getInt("rampDuration", 450)
            pauseMpris.checked = config.getBool("pauseMpris", true)
            voiceIsolation.checked = config.getBool("voiceIsolation", true)
        }

        QQC2.Slider {
            id: duckingSlider
            Kirigami.FormData.label: i18nd("kdeconnect-plugins", "Ducking Level:") + " " + Math.round(value) + "%"
            from: 0
            to: 50
            stepSize: 5
            onMoved: config.set("duckingLevel", Math.round(value))
        }

        QQC2.Slider {
            id: rampSlider
            Kirigami.FormData.label: i18nd("kdeconnect-plugins", "Smooth Transition:") + " " + Math.round(value) + " ms"
            from: 100
            to: 1000
            stepSize: 50
            onMoved: config.set("rampDuration", Math.round(value))
        }

        QQC2.CheckBox {
            id: pauseMpris
            text: i18nd("kdeconnect-plugins", "Pause MPRIS media players (Spotify, browser, etc.)")
            onClicked: config.set("pauseMpris", checked)
        }

        QQC2.CheckBox {
            id: voiceIsolation
            text: i18nd("kdeconnect-plugins", "Activate EasyEffects Voice Isolation DSP during calls")
            onClicked: config.set("voiceIsolation", checked)
        }
    }
}
