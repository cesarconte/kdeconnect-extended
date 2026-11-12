/**
 * SPDX-FileCopyrightText: 2022 Yuchen Shi <bolshaya_schists@mail.gravitide.co>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Clipboard plugin configuration page, migrated to the CachyOS.UI design
 * system (Golden Gate / Liquid Glass).
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kdeconnect
import CachyOS.UI

CachyFormCardPage {
    id: root

    property string device
    readonly property string max_clipboard_file_size: "maxClipboardFileSizeMB"

    KdeConnectPluginConfig {
        id: config
        deviceId: root.device
        pluginName: "kdeconnect_clipboard"
    }

    Component.onCompleted: {
        autoShare.checked = config.getBool("autoShare", config.getBool("sendUnknown", true))
        password.checked = config.getBool("sendPassword", true)
        cleanUrls.checked = config.getBool("cleanUrls", true)
        ocrEnabled.checked = config.getBool("ocrEnabled", true)
        autoCopyOcrText.checked = config.getBool("autoCopyOcrText", false)
        maxClipboardFileSizeMB.value = config.getInt(max_clipboard_file_size, 50)
    }

    CachyFormCard {
        Layout.fillWidth: true

        CachyFormCheckDelegate {
            id: autoShare
            text: i18nd("kdeconnect-plugins", "Automatically share the clipboard from this device")
            focus: true
            onToggled: (next) => {
                checked = next
                config.set("autoShare", next)
            }
            KeyNavigation.down: password
        }

        CachyFormDelegateSeparator {}

        CachyFormCheckDelegate {
            id: password
            text: i18nd("kdeconnect-plugins", "Including passwords (as marked by password managers)")
            onToggled: (next) => {
                checked = next
                config.set("sendPassword", next)
            }
        }

        CachyFormDelegateSeparator {}

        CachyFormCheckDelegate {
            id: cleanUrls
            text: i18nd("kdeconnect-plugins", "Clean tracking parameters from copied URLs (Clean Clipboard)")
            onToggled: (next) => {
                checked = next
                config.set("cleanUrls", next)
            }
        }

        CachyFormDelegateSeparator {}

        CachyFormCheckDelegate {
            id: ocrEnabled
            text: i18nd("kdeconnect-plugins", "Extract text from clipboard images automatically (On-the-Fly OCR)")
            onToggled: (next) => {
                checked = next
                config.set("ocrEnabled", next)
            }
        }

        CachyFormDelegateSeparator {}

        CachyFormCheckDelegate {
            id: autoCopyOcrText
            text: i18nd("kdeconnect-plugins", "Automatically copy extracted OCR text to clipboard")
            onToggled: (next) => {
                checked = next
                config.set("autoCopyOcrText", next)
            }
        }

        CachyFormDelegateSeparator {}

        RowLayout {
            Layout.fillWidth: true
            spacing: CachyTheme.spacingMedium

            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: i18nd("kdeconnect-plugins", "Max Clipboard File Size (MB)")
                wrapMode: Text.WordWrap
                font.pixelSize: 12
                color: CachyTheme.textPrimary
            }

            QQC2.SpinBox {
                id: maxClipboardFileSizeMB
                from: 0
                to: 1000000
                onValueModified: config.set(root.max_clipboard_file_size, value)
            }
        }
    }
}
