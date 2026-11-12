/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Smart Ducking plugin configuration page, migrated to the CachyOS.UI design
 * system (Golden Gate / Liquid Glass).
 */

import QtQuick
import QtQuick.Layouts
import org.kde.kdeconnect
import CachyOS.UI

CachyFormCardPage {
    id: root

    property string device

    KdeConnectPluginConfig {
        id: config
        deviceId: root.device
        pluginName: "kdeconnect_smartducking"
    }

    Component.onCompleted: {
        duckingSlider.value = config.getInt("duckingLevel", 15)
        rampSlider.value = config.getInt("rampDuration", 450)
        pauseMpris.checked = config.getBool("pauseMpris", true)
        voiceIsolation.checked = config.getBool("voiceIsolation", true)
    }

    CachyFormCard {
        Layout.fillWidth: true

        Text {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: i18nd("kdeconnect-plugins", "Ducking Level:") + " " + Math.round(duckingSlider.value) + "%"
            font.pixelSize: 12
            color: CachyTheme.textSecondary
        }

        CachySlider {
            id: duckingSlider
            Layout.fillWidth: true
            from: 0
            to: 50
            stepSize: 5
            onMoved: config.set("duckingLevel", Math.round(value))
        }

        CachyFormDelegateSeparator {}

        Text {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: i18nd("kdeconnect-plugins", "Smooth Transition:") + " " + Math.round(rampSlider.value) + " ms"
            font.pixelSize: 12
            color: CachyTheme.textSecondary
        }

        CachySlider {
            id: rampSlider
            Layout.fillWidth: true
            from: 100
            to: 1000
            stepSize: 50
            onMoved: config.set("rampDuration", Math.round(value))
        }

        CachyFormDelegateSeparator {}

        CachyFormCheckDelegate {
            id: pauseMpris
            text: i18nd("kdeconnect-plugins", "Pause MPRIS media players (Spotify, browser, etc.)")
            onToggled: (next) => {
                checked = next
                config.set("pauseMpris", next)
            }
        }

        CachyFormDelegateSeparator {}

        CachyFormCheckDelegate {
            id: voiceIsolation
            text: i18nd("kdeconnect-plugins", "Activate EasyEffects Voice Isolation DSP during calls")
            onToggled: (next) => {
                checked = next
                config.set("voiceIsolation", next)
            }
        }
    }
}
