/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Spatial Presence plugin configuration page, migrated to the CachyOS.UI
 * design system (Golden Gate / Liquid Glass).
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kdeconnect
import CachyOS.UI

CachyFormCardPage {
    id: root

    property string device

    KdeConnectPluginConfig {
        id: config
        deviceId: root.device
        pluginName: "kdeconnect_spatialpresence"
    }

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

    CachyFormHeader {
        Layout.fillWidth: true
        title: i18nd("kdeconnect-plugins", "Attention & Presence Tracking:")
    }

    CachyFormCard {
        Layout.fillWidth: true

        CachyFormCheckDelegate {
            id: presenceDimming
            text: i18nd("kdeconnect-plugins", "Dim screen when looking away or absent (Silent Dimming)")
            onToggled: (next) => {
                checked = next
                config.set("presenceDimmingEnabled", next)
            }
        }

        CachyFormDelegateSeparator {}

        RowLayout {
            Layout.fillWidth: true
            spacing: CachyTheme.spacingMedium

            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: i18nd("kdeconnect-plugins", "Absence before dimming (seconds):")
                wrapMode: Text.WordWrap
                font.pixelSize: 12
                color: CachyTheme.textPrimary
            }

            QQC2.SpinBox {
                id: dimTimeout
                from: 5
                to: 120
                stepSize: 5
                onValueModified: config.set("dimTimeoutSeconds", value)
            }
        }

        CachyFormDelegateSeparator {}

        RowLayout {
            Layout.fillWidth: true
            spacing: CachyTheme.spacingMedium

            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: i18nd("kdeconnect-plugins", "Dim brightness level (%):")
                wrapMode: Text.WordWrap
                font.pixelSize: 12
                color: CachyTheme.textPrimary
            }

            QQC2.SpinBox {
                id: dimBrightness
                from: 5
                to: 50
                stepSize: 5
                onValueModified: config.set("dimBrightnessPercent", value)
            }
        }

        CachyFormDelegateSeparator {}

        CachyFormCheckDelegate {
            id: presenceLock
            text: i18nd("kdeconnect-plugins", "Lock session upon prolonged absence")
            onToggled: (next) => {
                checked = next
                config.set("presenceLockEnabled", next)
            }
        }

        CachyFormDelegateSeparator {}

        RowLayout {
            Layout.fillWidth: true
            spacing: CachyTheme.spacingMedium

            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: i18nd("kdeconnect-plugins", "Absence before lock (seconds):")
                wrapMode: Text.WordWrap
                font.pixelSize: 12
                color: CachyTheme.textPrimary
            }

            QQC2.SpinBox {
                id: lockTimeout
                from: 30
                to: 600
                stepSize: 15
                onValueModified: config.set("lockTimeoutSeconds", value)
            }
        }
    }

    CachyFormHeader {
        Layout.fillWidth: true
        title: i18nd("kdeconnect-plugins", "Touchless Air Gestures:")
    }

    CachyFormCard {
        Layout.fillWidth: true

        CachyFormCheckDelegate {
            id: waveGestures
            text: i18nd("kdeconnect-plugins", "Wave left/right to switch virtual desktops in KWin")
            onToggled: (next) => {
                checked = next
                config.set("waveGesturesEnabled", next)
            }
        }

        CachyFormDelegateSeparator {}

        CachyFormCheckDelegate {
            id: micHoldGesture
            text: i18nd("kdeconnect-plugins", "Hold palm over camera to mute/unmute microphone")
            onToggled: (next) => {
                checked = next
                config.set("micHoldGestureEnabled", next)
            }
        }

        CachyFormDelegateSeparator {}

        CachyFormCheckDelegate {
            id: volumeGestures
            text: i18nd("kdeconnect-plugins", "Wave up/down to increase/decrease volume")
            onToggled: (next) => {
                checked = next
                config.set("volumeGesturesEnabled", next)
            }
        }
    }
}
