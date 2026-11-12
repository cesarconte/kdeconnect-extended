/**
 * SPDX-FileCopyrightText: 2019 Nicolas Fella <nicolas.fella@gmx.de>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Send-notifications plugin configuration page, migrated to the CachyOS.UI
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
        pluginName: "kdeconnect_sendnotifications"
    }

    Component.onCompleted: {
        persistent.checked = config.getBool("generalPersistent", false)
        includeBody.checked = config.getBool("generalIncludeBody", true)
        includeIcon.checked = config.getBool("generalSynchronizeIcons", true)
        urgency.value = config.getInt("generalUrgency", 0)
    }

    CachyFormCard {
        Layout.fillWidth: true

        CachyFormCheckDelegate {
            id: persistent
            text: i18nd("kdeconnect-plugins", "Persistent notifications only")
            focus: true
            onToggled: (next) => {
                checked = next
                config.set("generalPersistent", next)
            }
            KeyNavigation.down: includeBody
        }

        CachyFormDelegateSeparator {}

        CachyFormCheckDelegate {
            id: includeBody
            text: i18nd("kdeconnect-plugins", "Include body")
            onToggled: (next) => {
                checked = next
                config.set("generalIncludeBody", next)
            }
            KeyNavigation.down: includeIcon
        }

        CachyFormDelegateSeparator {}

        CachyFormCheckDelegate {
            id: includeIcon
            text: i18nd("kdeconnect-plugins", "Include icon")
            onToggled: (next) => {
                checked = next
                config.set("generalSynchronizeIcons", next)
            }
            KeyNavigation.down: urgency
        }

        CachyFormDelegateSeparator {}

        RowLayout {
            Layout.fillWidth: true
            spacing: CachyTheme.spacingMedium

            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: i18nd("kdeconnect-plugins", "Minimum urgency level:")
                font.pixelSize: 12
                color: CachyTheme.textPrimary
            }

            QQC2.SpinBox {
                id: urgency
                from: 0
                to: 2
                onValueModified: config.set("generalUrgency", value)
            }
        }
    }
}
