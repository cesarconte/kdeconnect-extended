/**
 * SPDX-FileCopyrightText: 2019 Nicolas Fella <nicolas.fella@gmx.de>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Find-this-device plugin configuration page, migrated to the CachyOS.UI
 * design system (Golden Gate / Liquid Glass).
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs as Dialogs
import QtMultimedia
import org.kde.kirigami as Kirigami
import org.kde.kdeconnect
import org.kde.kdeconnect.private.findthisdevice as FindThisDevice
import CachyOS.UI

CachyFormCardPage {
    id: root

    property string device

    readonly property list<Kirigami.Action> headerActions: [
        Kirigami.Action {
            icon.name: "dialog-ok"
            text: i18n("Apply")
            onTriggered: config.set("ringtone", path.text)
        }
    ]

    Dialogs.FileDialog {
        id: fileDialog
        selectedFile: "file://" + path.text
        onAccepted: {
            path.text = selectedFile.toString().replace("file://", "")
        }
    }

    KdeConnectPluginConfig {
        id: config
        deviceId: root.device
        pluginName: "kdeconnect_findthisdevice"

        onConfigChanged: {
            path.text = getString("ringtone", FindThisDevice.FindThisDeviceHelper.defaultSound())
        }
    }

    CachyFormCard {
        Layout.fillWidth: true

        Text {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: i18n("Sound to play:")
            font.pixelSize: 12
            color: CachyTheme.textSecondary
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: CachyTheme.spacingSmall

            CachyTextField {
                id: path
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                focus: true
                KeyNavigation.right: filePickerButton
            }

            CachyButton {
                id: filePickerButton
                iconName: "document-open-symbolic"
                onClicked: fileDialog.open()
                KeyNavigation.right: playButton
            }

            CachyButton {
                id: playButton
                iconName: playMusic.playing ? "media-playback-stop-symbolic" : "media-playback-start-symbolic"
                enabled: FindThisDevice.FindThisDeviceHelper.pathExists(path.text)
                onClicked: {
                    playMusic.source = path.text;
                    playMusic.playing ? playMusic.stop() : playMusic.play();
                }

                MediaPlayer {
                    id: playMusic
                    audioOutput: AudioOutput {}
                }
            }
        }
    }
}
