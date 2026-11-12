/*
 * SPDX-FileCopyrightText: 2015 Aleix Pol Gonzalez <aleixpol@kde.org>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Window of the KDE Connect application, migrated to the CachyOS.UI design
 * system (Golden Gate / Liquid Glass):
 *
 *  - the window is a `CachyWindow`: translucent frame with the KWin blur, a
 *    44 px title bar with the hard 1 px divider and the 20 px squircle radius;
 *  - the device list becomes the edge-to-edge sidebar of the window;
 *  - the navigation model (`Kirigami.PageStack`) is preserved untouched, only
 *    its chrome is replaced by the Golden Gate header.
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kdeconnect
import org.kde.config as KConfig
import CachyOS.UI

CachyWindow {
    id: root
    property int columnWidth: 240

    property string pendingDeviceId
    property string pendingPluginConfig

    width: 960
    height: 660
    minimumWidth: 640
    minimumHeight: 480
    visible: true

    showTrafficLights: true
    showTitleBarDivider: true
    showWindowFrame: true
    resizable: true
    cornerRadius: CachyTheme.radiusWindow
    // The content keeps the canonical margin on the four sides: only the
    // edge-to-edge sidebar reaches the divider of the header.
    contentPadded: true
    sidebarWidth: 300

    title: pageStack.currentItem && pageStack.currentItem.title
        ? pageStack.currentItem.title
        : i18nd("kdeconnect-app", "KDE Connect")

    KConfig.WindowStateSaver {
        configGroupName: "MainWindow"
    }

    Shortcut {
        sequences: [StandardKey.Quit]
        onActivated: root.close()
    }

    Component {
        id: deviceComp
        DevicePage {}
    }

    function openDevice(deviceId, pluginConfig) {
        if (devicesModel.busy) {
            root.pendingDeviceId = deviceId;
            root.pendingPluginConfig = pluginConfig;
            return;
        }

        root.pendingDeviceId = "";
        root.pendingPluginConfig = "";

        pageStack.clear();
        let device = devicesModel.deviceForId(deviceId);
        if (!device) {
            console.warn("Could not find", deviceId);
            return;
        }
        let devicePage = pageStack.push(deviceComp, {
            currentDevice: device,
            pageStack: pageStack
        });

        if (pluginConfig) {
            let settingsPage = devicePage.openSettings();
            settingsPage.openConfiguration(pluginConfig, pluginConfig)
        }
    }

    DevicesModel {
        id: devicesModel
        onBusyChanged: (busy) => {
            if (!busy && root.pendingDeviceId) {
                root.openDevice(root.pendingDeviceId, root.pendingPluginConfig);
            }
        }
    }

    // =========================================================================
    // CABECERA — navegación entre páginas
    // =========================================================================
    headerLeading: [
        RowLayout {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6

            CachyToolbarButton {
                iconName: "go-previous-symbolic"
                visible: pageStack.depth > 1
                tooltipText: i18nc("@action:button", "Back")
                onClicked: pageStack.removePage(pageStack.currentIndex)
            }
        }
    ]

    // =========================================================================
    // CABECERA — acciones de la página activa
    // =========================================================================
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
                    primary: false

                    onClicked: modelData.trigger()
                }
            }
        }
    ]

    // =========================================================================
    // BARRA LATERAL — lista de dispositivos
    // =========================================================================
    sidebarHeaderActions: [
        Row {
            spacing: 6
            anchors.verticalCenter: parent.verticalCenter

            CachyToolbarButton {
                iconName: "view-refresh-symbolic"
                tooltipText: i18nc("@action:button", "Refresh")
                onClicked: {
                    //refresh
                    DaemonDbusInterface.forceOnNetworkChange();
                }
            }
        }
    ]

    sidebar: [
        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.bottomMargin: 12
            spacing: CachyTheme.spacingSmall

            CachyHeading {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.topMargin: 8
                Layout.bottomMargin: 4
                text: i18nd("kdeconnect-app", "Devices")
                level: 3
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 0
                clip: true

                ListView {
                    id: devices
                    anchors.fill: parent
                    activeFocusOnTab: true
                    keyNavigationEnabled: true
                    Accessible.role: Accessible.List

                    section {
                        property: "status"
                        delegate: CachySectionHeader {
                            width: ListView.view.width
                            text: switch (parseInt(section)) {
                            case DevicesModel.Paired:
                                return i18nd("kdeconnect-app", "Remembered");
                            case DevicesModel.Reachable:
                                return i18nd("kdeconnect-app", "Available");
                            case (DevicesModel.Reachable | DevicesModel.Paired):
                                return i18nd("kdeconnect-app", "Connected");
                            }
                        }
                    }

                    model: DevicesSortProxyModel {
                        sourceModel: devicesModel
                    }

                    delegate: QQC2.ItemDelegate {
                        id: delegate
                        icon.name: iconName
                        text: Kirigami.MnemonicData.richTextLabel
                        Accessible.name: Kirigami.MnemonicData.plainTextLabel ?? model.name // fallback needed for KF < 6.12
                        width: ListView.view.width
                        highlighted: (pageStack.currentItem as DevicePage)?.currentDevice === device
                        Accessible.role: Accessible.PageTab
                        Accessible.description: toolTip

                        Kirigami.MnemonicData.enabled: enabled && visible
                        Kirigami.MnemonicData.controlType: Kirigami.MnemonicData.MenuItem
                        Kirigami.MnemonicData.label: model.name

                        Shortcut {
                            sequence: delegate.Kirigami.MnemonicData.sequence
                            onActivated: clicked()
                        }

                        contentItem: CachyIconTitleSubtitle {
                            title: delegate.text
                            subtitle: delegate.toolTip ?? ""
                            selected: delegate.highlighted
                            icon: delegate.icon
                            onClicked: delegate.clicked()
                        }

                        background: Item {}
                        padding: 0

                        onClicked: {
                            pageStack.clear();
                            pageStack.push(deviceComp, {
                                currentDevice: device,
                                pageStack: pageStack
                            });
                        }
                    }
                }

                CachyPlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - (CachyTheme.marginWindow * 2)
                    visible: devices.count === 0 && !devicesModel.busy
                    text: i18nd("kdeconnect-app", "No devices found")
                    iconName: "edit-none-symbolic"
                }

                QQC2.BusyIndicator {
                    anchors.centerIn: parent
                    visible: devicesModel.busy
                }
            }

            // --- Pie: nombre anunciado y ajustes ---
            RowLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 6

                Text {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: announcedNameProperty.value
                    elide: Text.ElideRight
                    font.pixelSize: 11
                    color: CachyTheme.textSecondary

                    DBusProperty {
                        id: announcedNameProperty
                        object: DaemonDbusInterface
                        read: "announcedName"
                        defaultValue: ""
                    }
                }

                CachyToolbarButton {
                    iconName: "settings-configure-symbolic"
                    tooltipText: i18nc("@action:button", "Settings")
                    onClicked: pageStack.pushDialogLayer(Qt.resolvedUrl("Settings.qml"), {
                        pageStack: pageStack
                    }, {
                        title: i18n("Settings")
                    })
                }
            }
        }
    ]

    // =========================================================================
    // CONTENIDO — pila de páginas (modelo de navegación intacto)
    // =========================================================================
    contentData: [
        CachyPageStack {
            id: pageStack
            anchors.fill: parent

            initialPage: Qt.resolvedUrl("WelcomePage.qml")

            Keys.onEscapePressed: {
                if (pageStack.currentIndex) {
                    pageStack.removePage(pageStack.currentIndex)
                }
            }
        }
    ]

    Component.onCompleted: devices.forceActiveFocus()
}
