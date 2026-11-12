/**
 * SPDX-FileCopyrightText: 2018 Aleix Pol Gonzalez <aleixpol@kde.org>
 * SPDX-FileCopyrightText: 2018 Nicolas Fella <nicolas.fella@gmx.de>
 * SPDX-FileCopyrightText: 2018 Simon Redman <simon@ergotech.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Window of the KDE Connect SMS application, migrated to the CachyOS.UI design
 * system (Golden Gate / Liquid Glass).
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kdeconnect
import org.kde.kdeconnect.sms
import org.kde.config as KConfig
import CachyOS.UI

CachyWindow {
    id: root

    visible: true
    width: 900
    height: 640
    minimumWidth: 640
    minimumHeight: 480

    showTrafficLights: true
    showTitleBarDivider: true
    showWindowFrame: true
    resizable: true
    cornerRadius: CachyTheme.radiusWindow
    contentPadded: true

    title: pageStack.currentItem && pageStack.currentItem.title
        ? pageStack.currentItem.title
        : i18nd("kdeconnect-sms", "KDE Connect SMS")

    property alias devicesCount: instantiator.count
    property var deviceActions: []

    KConfig.WindowStateSaver {
        configGroupName: "MainWindow"
    }

    Component {
        id: deviceActionComponent
        Kirigami.Action {
            required property string deviceId
            required property string name

            text: name

            onTriggered: {
                AppData.deviceId = deviceId
            }
            icon.name: AppData.deviceId === deviceId ? "checkmark" : ""
        }
    }

    Instantiator {
        id: instantiator

        model: DevicesPluginFilterProxyModel {
            id: devicesModel
            pluginFilter: "kdeconnect_sms"
            sourceModel: DevicesModel { displayFilter: DevicesModel.Paired | DevicesModel.Reachable }
        }

        onObjectAdded: (idx, obj) => {
            root.deviceActions.push(obj)

            if (!AppData.deviceId) {
                AppData.deviceId = obj.deviceId
            }
        }

        onObjectRemoved: (idx, obj) => {
            root.deviceActions.splice(idx, 1)
        }

        delegate: deviceActionComponent
    }

    Component {
        id: initialConversationList
        ConversationList {
            title: i18nd("kdeconnect-sms", "KDE Connect SMS")
            devicesCount: root.devicesCount
            pageStack: pageStack
        }
    }

    Component {
        id: aboutPageComponent
        CachyAboutPage {
            appTitle: i18nd("kdeconnect-sms", "KDE Connect SMS")
            appVersion: Qt.application.version
            iconName: "kdeconnect"
            description: i18nd("kdeconnect-sms", "SMS Instant Messaging")
            links: [
                { text: "KDE Connect", url: "https://kdeconnect.kde.org" }
            ]
            onCloseRequested: pageStack.layers.pop()
        }
    }

    // =========================================================================
    // CABECERA — navegación y acciones
    // =========================================================================
    headerLeading: [
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6

            CachyToolbarButton {
                iconName: "go-previous-symbolic"
                visible: pageStack.depth > 1 || pageStack.layers.depth > 0
                tooltipText: i18nd("kdeconnect-sms", "Back")
                onClicked: {
                    if (pageStack.layers.depth > 0) {
                        pageStack.layers.pop()
                    } else if (pageStack.depth > 1) {
                        pageStack.removePage(pageStack.currentIndex)
                    }
                }
            }
        }
    ]

    headerActions: [
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6

            Repeater {
                model: pageStack.currentItem && pageStack.currentItem.headerActions
                    ? pageStack.currentItem.headerActions
                    : []

                CachyToolbarButton {
                    required property var modelData

                    iconName: modelData.icon && modelData.icon.name ? modelData.icon.name : ""
                    tooltipText: modelData.text ? modelData.text : ""
                    visible: modelData.visible !== false
                    onClicked: modelData.trigger()
                }
            }

            CachyToolbarButton {
                iconName: "view-refresh-symbolic"
                tooltipText: i18nd("kdeconnect-sms", "Refresh")
                enabled: root.devicesCount > 0
                onClicked: {
                    if (pageStack.initialPageItem && pageStack.initialPageItem.conversationListModel) {
                        pageStack.initialPageItem.conversationListModel.refresh();
                    }
                }
            }

            CachyToolbarButton {
                iconName: "help-about-symbolic"
                tooltipText: i18nd("kdeconnect-sms", "About")
                onClicked: {
                    if (pageStack.layers.depth < 1) {
                        pageStack.layers.push(aboutPageComponent, {})
                    } else {
                        pageStack.layers.pop()
                    }
                }
            }
        }
    ]

    contentData: [
        CachyPageStack {
            id: pageStack
            anchors.fill: parent
            initialPage: initialConversationList
        }
    ]
}
