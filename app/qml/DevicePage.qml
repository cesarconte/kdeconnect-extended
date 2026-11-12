/*
 * SPDX-FileCopyrightText: 2015 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Device page of the KDE Connect application, migrated to the CachyOS.UI
 * design system (Golden Gate / Liquid Glass): the page keeps the model of the
 * plugins and the states of the pairing, only its appearance changes.
 */

import QtQuick
import QtCore
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQml.Models
import org.kde.kirigami as Kirigami
import org.kde.kdeconnect
import CachyOS.UI

Item {
    id: root

    /// The stack of pages of the window.
    property var pageStack: null

    property QtObject currentDevice

    readonly property string title: root.currentDevice ? root.currentDevice.name : ""

    /// Actions of the page: they are shown in the header of the window.
    readonly property list<Kirigami.Action> headerActions: [
        Kirigami.Action {
            icon.name: "network-disconnect-symbolic"
            text: i18nd("kdeconnect-app", "Unpair")
            visible: root.currentDevice && root.currentDevice.isPaired
            onTriggered: {
                root.currentDevice.unpair();
                root.pageStack.pop();
            }
        },
        Kirigami.Action {
            icon.name: "system-run-symbolic"
            text: i18nd("kdeconnect-app", "Send Ping")
            visible: root.currentDevice && root.currentDevice.isPaired && root.currentDevice.isReachable
            onTriggered: {
                root.currentDevice.pluginCall("ping", "sendPing");
            }
        },
        Kirigami.Action {
            icon.name: "settings-configure-symbolic"
            text: i18n("Plugin Settings")
            visible: root.currentDevice && root.currentDevice.isPaired && root.currentDevice.isReachable
            onTriggered: {
                root.openSettings();
            }
        }
    ]

    BatteryInfo {
        id: batteryInfo
        device: root.currentDevice
    }

    ConnectivityInfo {
        id: connectivityInfo
        device: root.currentDevice
    }

    function openSettings()
    {
        for (let index = 0; index < pageStack.depth; ++index) {
            const page = pageStack.get(index);
            if (page && page.isPluginSettingsPage && page.device === currentDevice.id()) {
                pageStack.currentIndex = index;
                return page;
            }
        }

        return pageStack.push(
            Qt.resolvedUrl("PluginSettings.qml"),
            {device: currentDevice.id(), pageStack: root.pageStack}
        );
    }

    ListView {
        id: pluginsListView
        anchors.fill: parent

        section.property: "section"
        section.delegate: CachySectionHeader {
            width: ListView.view.width
            text: switch (section) {
                case "action":
                    return i18nc("@title:group device page section header", "Actions");
                case "control":
                    return i18nc("@title:group device page section header", "Controls");
                case "info":
                    return i18nc("@title:group device page section header", "Information");
            }
        }
        Accessible.role: Accessible.List

        model: DelegateModel{
            id: pluginModel
            model: pluginsListView.plugins
            groups: [
                DelegateModelGroup {name: "loadedPlugins"}
            ]
            filterOnGroup: "loadedPlugins"
            property int numberPluginsLoaded: pluginsListView.plugins.filter(plugin => !plugin.hidden && (plugin.loaded || plugin.section === "info")).length ?? 0
            onNumberPluginsLoadedChanged: update()

            function update() {
                for (let i = 0; i < items.count; ++i) {
                    let item = items.get(i);
                    item.inLoadedPlugins = !item.model.hidden && (item.model.loaded || (item.model.section === "info" && root.currentDevice.isPaired))
                }
            }

            delegate: QQC2.ItemDelegate {
                id: pluginDelegate
                Accessible.name: Kirigami.MnemonicData.plainTextLabel ?? modelData.name // fallback needed for KF < 6.12
                Accessible.role: modelData.clickable ? Accessible.Button : Accessible.StaticText
                highlighted: modelData === sharePlugin && dropArea.containsDrag
                width: pluginsListView.width
                height: 44
                enabled: true
                hoverEnabled: modelData.clickable
                focusPolicy: modelData.clickable ? Qt.StrongFocus : Qt.NoFocus
                onClicked: {
                    if (modelData.clickable) {
                        modelData.onClick()
                    }
                }

                Kirigami.MnemonicData.enabled: modelData.clickable && visible
                Kirigami.MnemonicData.controlType: Kirigami.MnemonicData.MenuItem
                Kirigami.MnemonicData.label: modelData.name

                Shortcut {
                    sequence: pluginDelegate.Kirigami.MnemonicData.sequence
                    onActivated: clicked()
                }

                contentItem: RowLayout {
                    spacing: CachyTheme.spacingMedium

                    Image {
                        Layout.preferredWidth: 22
                        Layout.preferredHeight: 22
                        source: CachyTheme.iconUrl(modelData.iconName, "application-x-executable-symbolic")
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: pluginDelegate.Kirigami.MnemonicData.richTextLabel
                        elide: Text.ElideRight
                        font.pixelSize: 13
                        color: CachyTheme.textPrimary
                    }
                }

                background: Rectangle {
                    radius: CachyTheme.radiusSmall
                    color: pluginDelegate.highlighted
                        ? CachyTheme.withAlpha(CachyTheme.accentColor, 0.16)
                        : (pluginDelegate.hovered && modelData.clickable ? CachyTheme.cardBackgroundHover : "transparent")
                }

                MouseArea {
                    anchors.fill: parent
                    visible: !modelData.clickable
                    enabled: visible
                    acceptedButtons: Qt.AllButtons
                    hoverEnabled: true
                    preventStealing: true
                }
            }
        }

        property list<QtObject> plugins: [
            PluginItem {
                name: i18nd("kdeconnect-app", "Multimedia control")
                interfaceFactory: MprisDbusInterfaceFactory
                component: "mpris.qml"
                pluginName: "mprisremote"
                section: "control"
                device: root.currentDevice
            },
            PluginItem {
                name: i18nd("kdeconnect-app", "Notifications")
                interfaceFactory: DeviceNotificationsDbusInterfaceFactory
                component: "NotificationPanel.qml"
                pluginName: "notifications"
                section: "control"
                device: root.currentDevice
            },
            PluginItem {
                name: i18nd("kdeconnect-app", "Remote input")
                interfaceFactory: RemoteControlDbusInterfaceFactory
                component: "mousepad.qml"
                pluginName: "remotecontrol"
                section: "control"
                device: root.currentDevice
            },
            PluginItem {
                name: i18nd("kdeconnect-app", "Run command")
                interfaceFactory: RemoteCommandsDbusInterfaceFactory
                component: "runcommand.qml"
                pluginName: "remotecommands"
                section: "control"
                device: root.currentDevice
            },
            PluginItem {
                name: i18nd("kdeconnect-app", "Presentation Remote")
                interfaceFactory: RemoteKeyboardDbusInterfaceFactory
                component: "presentationRemote.qml"
                pluginName: "remotecontrol"
                section: "control"
                device: root.currentDevice
                hidden: root.currentDevice.type === "phone" || root.currentDevice.type === "tablet"
            },
            PluginItem {
                name: i18nd("kdeconnect-app", "Volume control")
                interfaceFactory: RemoteSystemVolumeDbusInterfaceFactory
                component: "volume.qml"
                pluginName: "remotesystemvolume"
                section: "control"
                device: root.currentDevice
            },
            PluginItem {
                readonly property QtObject sms: SmsDbusInterfaceFactory.create(root.currentDevice.id())
                pluginName: "sms"
                name: i18n("SMS Messages")
                onClick: () => sms.launchApp();
                section: "control"
                device: root.currentDevice
            },
            PluginItem {
                readonly property var lockIface: LockDeviceDbusInterfaceFactory.create(root.currentDevice.id())
                pluginName: "lockdevice"
                name: lockIface.isLocked ? i18nd("kdeconnect-app", "Unlock") : i18nd("kdeconnect-app", "Lock")
                onClick: () => lockIface.isLocked = !lockIface.isLocked;
                section: "action"
                device: root.currentDevice
            },
            PluginItem {
                readonly property var findmyphoneIface: FindMyPhoneDbusInterfaceFactory.create(root.currentDevice.id())
                pluginName: "findmyphone"
                name: i18nd("kdeconnect-app", "Find Device")
                section: "action"
                onClick: () => findmyphoneIface.ring()
                device: root.currentDevice
            },
            PluginItem {
                readonly property var clipboardIface: ClipboardDbusInterfaceFactory.create(root.currentDevice.id())
                pluginName: "clipboard"
                name: i18nd("kdeconnect-app", "Send Clipboard")
                onClick: () => clipboardIface.sendClipboard()
                section: "action"
                device: root.currentDevice
            },
            PluginItem {
                id: sharePlugin
                pluginName: "share"
                name: i18nd("kdeconnect-app", "Share File")
                onClick: () => fileDialog.open()
                section: "action"
                device: root.currentDevice
            },
            PluginItem {
                readonly property var sftp: SftpDbusInterfaceFactory.create(root.currentDevice.id())
                pluginName: "sftp"
                name: i18n("Browse this device")
                onClick: () => sftp.startBrowsing();
                section: "action"
                device: root.currentDevice
            },
            PluginItem {
                readonly property var virtualMonitor: VirtualmonitorDbusInterfaceFactory.create(root.currentDevice.id())
                pluginName: "virtualmonitor"
                name: virtualMonitor.active ? i18n("Stop virtual monitor session") : i18n("Use device as an external monitor")
                onClick: () => {
                    if (virtualMonitor.active) {
                        virtualMonitor.stop();
                    } else {
                        virtualMonitor.requestVirtualMonitor();
                        if (virtualMonitor.lastError.length > 0) {
                            showPassiveNotification(virtualMonitor.lastError)
                        }
                    }
                }
                section: "action"
                device: root.currentDevice
            },
            PluginItem {
                pluginName: "battery"
                name: batteryInfo.displayString
                iconNameOverride: batteryInfo.iconName
                hidden: !batteryInfo.hasBattery
                section: "info"
                device: root.currentDevice
            },
            PluginItem {
                pluginName: "connectivity_report"
                name: connectivityInfo.displayString
                iconNameOverride: connectivityInfo.iconName
                hidden: !connectivityInfo.available
                section: "info"
                device: root.currentDevice
            },
            PluginItem {
                name: i18nd("kdeconnect-app", "Address: %1", root.currentDevice.connectionDescription)
                hidden: !root.currentDevice.isReachable
                section: "info"
                device: root.currentDevice
            }
        ]

        DropArea {
            id: dropArea
            parent: pluginsListView
            anchors.fill: parent
            enabled: sharePlugin.loaded
            onEntered: drag => drag.accepted = drag.hasUrls
            onDropped: drop => {
                fileDialog.shareIface.shareUrls(drop.urls)
                drop.accept(Qt.CopyAction)
            }
        }
    }

    // =====================================================================
    // States of the pairing
    // =====================================================================
    CachyPlaceholderMessage {
        anchors.centerIn: parent
        width: parent.width - (CachyTheme.marginWindow * 2)
        visible: root.currentDevice && root.currentDevice.isReachable && !root.currentDevice.isPaired && !root.currentDevice.isPairRequestedByPeer && !root.currentDevice.isPairRequested
        text: i18nd("kdeconnect-app", "This device is not paired.")
        helpfulAction: ({
            text: i18ndc("kdeconnect-app", "Request pairing with a given device", "Pair"),
            onTriggered: () => root.currentDevice.requestPairing()
        })
    }

    CachyPlaceholderMessage {
        anchors.centerIn: parent
        width: parent.width - (CachyTheme.marginWindow * 2)
        visible: root.currentDevice && root.currentDevice.isReachable && root.currentDevice.isPairRequested
        text: i18nd("kdeconnect-app", "Pair requested\nKey: " + root.currentDevice.verificationKey)

        QQC2.BusyIndicator {
        }
    }

    CachyPlaceholderMessage {
        anchors.centerIn: parent
        width: parent.width - (CachyTheme.marginWindow * 2)
        visible: root.currentDevice && root.currentDevice.isPairRequestedByPeer
        text: i18n("Pair requested\nKey: " + root.currentDevice.verificationKey)

        RowLayout {
            spacing: CachyTheme.spacingSmall

            CachyButton {
                text: i18nd("kdeconnect-app", "Accept")
                primary: true
                onClicked: root.currentDevice.acceptPairing()
            }

            CachyButton {
                text: i18nd("kdeconnect-app", "Reject")
                onClicked: root.currentDevice.cancelPairing()
            }
        }
    }

    CachyPlaceholderMessage {
        anchors.centerIn: parent
        width: parent.width - (CachyTheme.marginWindow * 2)
        visible: root.currentDevice && !root.currentDevice.isReachable
        text: i18nd("kdeconnect-app", "This device is not reachable")
    }

    FileDialog {
        id: fileDialog
        readonly property var shareIface: root.currentDevice ? ShareDbusInterfaceFactory.create(root.currentDevice.id()) : null
        title: i18nd("kdeconnect-app", "Please choose a file")
        currentFolder: StandardPaths.standardLocations(StandardPaths.HomeLocation)[0]
        fileMode: FileDialog.OpenFiles
        onAccepted: shareIface.shareUrls(fileDialog.selectedFiles)
    }

    // The plugins need the stack of pages to open the page of their own
    // component, so they are given it as soon as it is known.
    onPageStackChanged: root.syncPlugins()
    Component.onCompleted: root.syncPlugins()

    function syncPlugins() {
        for (const plugin of pluginsListView.plugins) {
            plugin.pageStack = root.pageStack;
        }
    }
}
