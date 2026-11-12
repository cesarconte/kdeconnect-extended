/**
 * SPDX-FileCopyrightText: 2019 Nicolas Fella <nicolas.fella@gmx.de>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Share plugin configuration page, migrated to the CachyOS.UI design system
 * (Golden Gate / Liquid Glass).
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import org.kde.kirigami as Kirigami
import org.kde.kdeconnect
import CachyOS.UI

CachyFormCardPage {
    id: root

    property string device

    readonly property list<Kirigami.Action> headerActions: [
        Kirigami.Action {
            icon.name: "dialog-ok"
            text: i18nd("kdeconnect-plugins", "Apply")
            onTriggered: config.set("incoming_path", path.text)
        }
    ]

    FolderDialog {
        id: folderDialog
        currentFolder: path.text

        onAccepted: {
            path.text = selectedFolder.toString().replace("file://", "")
        }
    }

    KdeConnectPluginConfig {
        id: config
        deviceId: root.device
        pluginName: "kdeconnect_share"

        onConfigChanged: {
            path.text = getString("incoming_path", StandardPaths.writableLocation(StandardPaths.DownloadsLocation).toString().replace("file://", ""))
        }
    }

    CachyFormCard {
        Layout.fillWidth: true

        Text {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: i18nd("kdeconnect-plugins", "Save files in:")
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
                KeyNavigation.right: folderPickerButton
            }

            CachyButton {
                id: folderPickerButton
                iconName: "document-open"
                onClicked: folderDialog.open()
            }
        }

        Text {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: i18nd("kdeconnect-plugins", "%1 in the path will be replaced with the specific device name", "%1")
            wrapMode: Text.WordWrap
            font.pixelSize: 11
            color: CachyTheme.textMuted
        }
    }
}
