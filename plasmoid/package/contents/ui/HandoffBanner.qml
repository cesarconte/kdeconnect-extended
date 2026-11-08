/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kdeconnect as KDEConnect
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3

Rectangle {
    id: root

    property string deviceName: ""
    property string activityTitle: ""
    property string activityUrl: ""
    property string sourceApp: ""
    property bool hasActivity: activityUrl.length > 0

    signal openRequested()
    signal dismissRequested()

    visible: hasActivity
    implicitHeight: layout.implicitHeight + Kirigami.Units.smallSpacing * 2
    radius: Kirigami.Units.smallSpacing
    color: Kirigami.Theme.cardBackgroundColor
    border.color: Kirigami.Theme.highlightColor
    border.width: 1

    RowLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Kirigami.Units.smallSpacing
        spacing: Kirigami.Units.smallSpacing

        Kirigami.Icon {
            source: "internet-web-browser"
            implicitWidth: Kirigami.Units.iconSizes.medium
            implicitHeight: Kirigami.Units.iconSizes.medium
            color: Kirigami.Theme.highlightColor
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1

            PlasmaComponents3.Label {
                text: root.deviceName.length > 0
                    ? i18n("Continuar en este equipo desde %1", root.deviceName)
                    : i18n("Continuar en este equipo")
                font.bold: true
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            PlasmaComponents3.Label {
                text: root.activityTitle.length > 0 ? root.activityTitle : root.activityUrl
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                color: Kirigami.Theme.disabledTextColor
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        PlasmaComponents3.Button {
            text: i18n("Abrir")
            icon.name: "document-open"
            onClicked: {
                if (root.activityUrl.length > 0) {
                    Qt.openUrlExternally(root.activityUrl);
                }
                root.openRequested();
            }
        }

        PlasmaComponents3.ToolButton {
            icon.name: "dialog-close"
            display: PlasmaComponents3.AbstractButton.IconOnly
            onClicked: {
                root.hasActivity = false;
                root.dismissRequested();
            }
        }
    }
}
