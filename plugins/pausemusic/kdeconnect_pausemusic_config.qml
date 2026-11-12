/**
 * SPDX-FileCopyrightText: 2019 Nicolas Fella <nicolas.fella@gmx.de>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Pause-music plugin configuration page, migrated to the CachyOS.UI design
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
        pluginName: "kdeconnect_pausemusic"
    }

    Component.onCompleted: {
        talking.checked = config.getBool("conditionTalking", false)
        mute.checked = config.getBool("actionMute", false)
        pause.checked = config.getBool("actionPause", true)
        resume.checked = config.getBool("actionResume", true)
    }

    CachyFormCard {
        Layout.fillWidth: true

        CachyFormCheckDelegate {
            id: ringing
            text: i18nd("kdeconnect-plugins", "Pause as soon as phone rings")
            checked: !talking.checked
            focus: true
            onToggled: (next) => {
                talking.checked = !next
                config.set("conditionTalking", talking.checked)
            }
            KeyNavigation.down: talking
        }

        CachyFormDelegateSeparator {}

        CachyFormCheckDelegate {
            id: talking
            text: i18nd("kdeconnect-plugins", "Pause only while talking")
            onToggled: (next) => {
                checked = next
                config.set("conditionTalking", next)
            }
            KeyNavigation.down: pause
        }
    }

    CachyFormCard {
        Layout.fillWidth: true

        CachyFormCheckDelegate {
            id: pause
            text: i18nd("kdeconnect-plugins", "Pause media players")
            onToggled: (next) => {
                checked = next
                config.set("actionPause", next)
            }
            KeyNavigation.down: mute
        }

        CachyFormDelegateSeparator {}

        CachyFormCheckDelegate {
            id: mute
            text: i18nd("kdeconnect-plugins", "Mute system sound")
            onToggled: (next) => {
                checked = next
                config.set("actionMute", next)
            }
            KeyNavigation.down: resume
        }

        CachyFormDelegateSeparator {}

        CachyFormCheckDelegate {
            id: resume
            text: i18nd("kdeconnect-plugins", "Resume automatically when call ends")
            onToggled: (next) => {
                checked = next
                config.set("actionResume", next)
            }
        }
    }
}
