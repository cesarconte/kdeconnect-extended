/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Activity banner of the KDE Connect plasmoid, migrated to the CachyOS.UI
 * design system (Golden Gate / Liquid Glass).
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import org.kde.kdeconnect as KDEConnect
import org.kde.kirigami as Kirigami
import CachyOS.UI

Rectangle {
    id: root

    property KDEConnect.DevicesModel devicesModel
    property var activeWatcher: null

    property string deviceName: ""
    property string activityTitle: ""
    property string activityUrl: ""
    property string sourceApp: ""
    property bool hasActivity: activityUrl.length > 0

    signal openRequested()
    signal dismissRequested()

    visible: hasActivity
    implicitHeight: layout.implicitHeight + (CachyTheme.spacingSmall * 2)
    radius: CachyTheme.radiusMedium
    color: CachyTheme.cardBackground
    border.color: CachyTheme.accentColor
    border.width: 1

    Behavior on opacity {
        NumberAnimation { duration: CachyTheme.animFast }
    }

    Instantiator {
        id: handoffWatchers
        model: root.devicesModel

        delegate: Item {
            id: watcher
            visible: false
            required property string deviceId
            required property string name

            property KDEConnect.HandoffDbusInterface handoffIface: KDEConnect.HandoffDbusInterfaceFactory.create(deviceId)

            Component.onCompleted: {
                root.updateActiveActivity();
            }

            Connections {
                target: watcher.handoffIface
                function onActivityChangedProxy() {
                    root.updateActiveActivity();
                }
            }
        }

        onCountChanged: root.updateActiveActivity()
    }

    function updateActiveActivity() {
        for (let i = 0; i < handoffWatchers.count; ++i) {
            let item = handoffWatchers.objectAt(i);
            if (item && item.handoffIface && item.handoffIface.hasActiveActivity) {
                root.activeWatcher = item;
                root.deviceName = item.name;
                root.activityTitle = item.handoffIface.currentTitle;
                root.activityUrl = item.handoffIface.currentUrl;
                root.sourceApp = item.handoffIface.sourceApp;
                root.hasActivity = true;
                return;
            }
        }
        root.hasActivity = false;
        root.activeWatcher = null;
        root.activityUrl = "";
        root.activityTitle = "";
        root.deviceName = "";
        root.sourceApp = "";
    }

    RowLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: CachyTheme.spacingSmall
        spacing: CachyTheme.spacingSmall

        Kirigami.Icon {
            source: "internet-web-browser"
            implicitWidth: 24
            implicitHeight: 24
            color: CachyTheme.accentColor
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 1

            Text {
                text: root.deviceName.length > 0
                    ? i18n("Continuar en este equipo desde %1", root.deviceName)
                    : i18n("Continuar en este equipo")
                font.bold: true
                font.pixelSize: 11
                color: CachyTheme.textPrimary
                elide: Text.ElideRight
                Layout.fillWidth: true
                Layout.minimumWidth: 0
            }

            Text {
                text: root.activityTitle.length > 0 ? root.activityTitle : root.activityUrl
                font.pixelSize: 11
                color: CachyTheme.textMuted
                elide: Text.ElideRight
                Layout.fillWidth: true
                Layout.minimumWidth: 0
            }
        }

        CachyButton {
            text: i18n("Abrir")
            iconName: "document-open"
            primary: true
            onClicked: {
                if (root.activeWatcher && root.activeWatcher.handoffIface) {
                    root.activeWatcher.handoffIface.openCurrentActivity();
                } else if (root.activityUrl.length > 0) {
                    Qt.openUrlExternally(root.activityUrl);
                }
                root.hasActivity = false;
                root.openRequested();
            }
        }

        CachyToolbarButton {
            iconName: "dialog-close"
            tooltipText: i18n("Descartar actividad")
            onClicked: {
                if (root.activeWatcher && root.activeWatcher.handoffIface) {
                    root.activeWatcher.handoffIface.clearActivity();
                }
                root.hasActivity = false;
                root.dismissRequested();
            }
        }
    }
}
