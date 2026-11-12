// SPDX-FileCopyrightText: 2022 Carl Schwan <carl@carlschwan.eu>
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
//
// Settings page of the KDE Connect application, migrated to the CachyOS.UI
// design system (Golden Gate / Liquid Glass).

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kdeconnect
import org.kde.kdeconnect.app
import CachyOS.UI

CachyScrollablePage {
    id: page

    /// The stack of pages of the window: it is the one that owns the modal
    /// layers where the "about" pages are shown.
    property var pageStack: null

    title: i18nc("@title:window", "Settings")

    DBusProperty {
        id: announcedNameProperty
        object: DaemonDbusInterface
        read: "announcedName"
        defaultValue: ""
    }

    DBusProperty {
        id: linkProvidersProperty
        object: DaemonDbusInterface
        read: "linkProviders"
        defaultValue: []
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        spacing: CachyTheme.spacingMedium

        CachyFormCard {
            Layout.fillWidth: true

            CachyFormTextFieldDelegate {
                id: deviceNameField
                text: announcedNameProperty.value
                onAccepted: DaemonDbusInterface.setAnnouncedName(text)
                label: i18n("Device name")
            }
        }

        CachyFormHeader {
            title: i18nc("@title:group", "Backends")
            Layout.fillWidth: true
        }

        CachyFormCard {
            Layout.fillWidth: true
            visible: linkProvidersProperty.value.length > 0

            Repeater {
                model: linkProvidersProperty.value

                CachyFormCheckDelegate {
                    required property string modelData

                    readonly property string displayName: modelData.split('|')[0]
                    readonly property string internalName: modelData.split('|')[1]

                    checked: modelData.split('|')[2] === 'enabled'
                    text: displayName

                    onToggled: (next) => DaemonDbusInterface.setLinkProviderState(internalName, next)
                }
            }
        }

        CachyFormCard {
            Layout.fillWidth: true
            Layout.topMargin: CachyTheme.spacingMedium

            CachyFormButtonDelegate {
                text: i18n("About KDE Connect")
                icon.name: "kdeconnect"
                onClicked: page.pageStack.layers.push(aboutApplication, {
                    appTitle: i18n("KDE Connect"),
                    appVersion: Qt.application.version,
                    iconName: "kdeconnect",
                    description: i18n("KDE Connect lets you share files, the clipboard, the notifications and the control of your devices between your computers and your mobile devices."),
                    links: [
                        { text: i18n("Homepage"), url: "https://kdeconnect.kde.org" },
                        { text: i18n("Release notes"), url: "https://kdeconnect.kde.org/release-notes" }
                    ]
                })
            }

            CachyFormDelegateSeparator {}

            CachyFormButtonDelegate {
                text: i18n("About KDE")
                icon.name: "kde"
                onClicked: page.pageStack.layers.push(aboutKDE, {
                    appTitle: i18n("KDE"),
                    iconName: "kde",
                    description: i18n("KDE is an international community of people who build and use free software. You can learn more about it and support it."),
                    links: [
                        { text: i18n("Homepage"), url: "https://kde.org" },
                        { text: i18n("Applications"), url: "https://apps.kde.org" }
                    ]
                })
            }
        }
    }

    Component {
        id: aboutApplication
        CachyAboutPage {}
    }

    Component {
        id: aboutKDE
        CachyAboutPage {}
    }

    footer: Item {
        implicitHeight: 56

        CachyButton {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            text: i18n("Close")
            onClicked: {
                DaemonDbusInterface.setAnnouncedName(deviceNameField.text);
                page.pageStack.layers.pop();
            }
        }
    }
}
