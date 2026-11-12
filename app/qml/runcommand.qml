/*
 * SPDX-FileCopyrightText: 2018 Nicolas Fella <nicolas.fella@gmx.de>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Remote commands page of the KDE Connect application, migrated to the
 * CachyOS.UI design system (Golden Gate / Liquid Glass).
 */

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kdeconnect
import CachyOS.UI

Item {
    id: root

    property string title: i18nd("kdeconnect-app", "Run command")
    property QtObject pluginInterface
    property QtObject device

    readonly property list<Kirigami.Action> headerActions: [
        Kirigami.Action {
            icon.name: "document-edit"
            text: i18nd("kdeconnect-app", "Edit commands")
            onTriggered: {
                root.pluginInterface.editCommands();
            }
        }
    ]

    ListView {
        id: commandsList
        anchors.fill: parent
        spacing: CachyTheme.spacingTiny
        model: RemoteCommandsModel {
            deviceId: root.pluginInterface.deviceId
        }
        delegate: QQC2.ItemDelegate {
            id: delegate
            required property string name
            required property string command
            required property string key

            width: ListView.view.width
            text: name

            contentItem: CachyIconTitleSubtitle {
                title: delegate.name
                subtitle: delegate.command
                onClicked: delegate.clicked()
            }

            background: Item {}
            padding: 0

            onClicked: root.pluginInterface.triggerCommand(delegate.key)
        }
    }

    CachyPlaceholderMessage {
        anchors.centerIn: parent
        width: parent.width - (CachyTheme.marginWindow * 2)
        visible: commandsList.count === 0
        iconName: "utilities-terminal"
        text: i18nd("kdeconnect-app", "No commands defined")
    }
}
