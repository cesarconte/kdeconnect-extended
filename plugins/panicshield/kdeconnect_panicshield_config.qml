/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Panic Shield plugin configuration page, migrated to the CachyOS.UI design
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
        pluginName: "kdeconnect_panicshield"
    }

    Component.onCompleted: {
        hideDesktop.checked = config.getBool("hideDesktop", true)
        muteAudio.checked = config.getBool("muteAudio", true)
        pauseMedia.checked = config.getBool("pauseMedia", true)
        inhibitNotifications.checked = config.getBool("inhibitNotifications", true)
        lockSession.checked = config.getBool("lockSession", false)
    }

    CachyFormHeader {
        Layout.fillWidth: true
        title: i18nd("kdeconnect-plugins", "Emergency Privacy Actions:")
    }

    CachyFormCard {
        Layout.fillWidth: true

        CachyFormCheckDelegate {
            id: hideDesktop
            text: i18nd("kdeconnect-plugins", "Hide all open windows (Show Desktop / Boss Key)")
            onToggled: (next) => {
                checked = next
                config.set("hideDesktop", next)
            }
        }

        CachyFormDelegateSeparator {}

        CachyFormCheckDelegate {
            id: muteAudio
            text: i18nd("kdeconnect-plugins", "Instantly mute system audio (PipeWire)")
            onToggled: (next) => {
                checked = next
                config.set("muteAudio", next)
            }
        }

        CachyFormDelegateSeparator {}

        CachyFormCheckDelegate {
            id: pauseMedia
            text: i18nd("kdeconnect-plugins", "Pause all active media playback (MPRIS)")
            onToggled: (next) => {
                checked = next
                config.set("pauseMedia", next)
            }
        }

        CachyFormDelegateSeparator {}

        CachyFormCheckDelegate {
            id: inhibitNotifications
            text: i18nd("kdeconnect-plugins", "Suppress popup notifications (Do Not Disturb)")
            onToggled: (next) => {
                checked = next
                config.set("inhibitNotifications", next)
            }
        }

        CachyFormDelegateSeparator {}

        CachyFormCheckDelegate {
            id: lockSession
            text: i18nd("kdeconnect-plugins", "Lock screen session immediately upon panic activation")
            onToggled: (next) => {
                checked = next
                config.set("lockSession", next)
            }
        }
    }
}
