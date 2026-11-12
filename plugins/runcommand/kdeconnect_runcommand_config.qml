/**
 * SPDX-FileCopyrightText: 2019 Nicolas Fella <nicolas.fella@gmx.de>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Run-command plugin configuration page, migrated to the CachyOS.UI design
 * system (Golden Gate / Liquid Glass).
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kdeconnect
import CachyOS.UI

Item {
    id: root

    property string device
    property string title

    readonly property list<Kirigami.Action> headerActions: [
        Kirigami.Action {
            id: addCommandDialogAction
            icon.name: "list-add"
            text: i18nd("kdeconnect-plugins", "Add Command…")
            onTriggered: {
                nameField.text = ""
                commandField.text = ""
                addDialog.open()
            }
        }
    ]

    ListView {
        id: view
        anchors.fill: parent
        spacing: CachyTheme.spacingSmall
        focus: view.count > 0

        model: CommandsModel {
            id: commandModel
            deviceId: root.device
        }

        delegate: QQC2.ItemDelegate {
            id: commandDelegate

            width: ListView.view.width
            height: 52

            required property var model

            KeyNavigation.tab: editButton
            KeyNavigation.right: editButton

            background: Rectangle {
                radius: CachyTheme.radiusSmall
                color: commandDelegate.hovered ? CachyTheme.cardBackgroundHover : CachyTheme.cardBackground
                border.width: 1
                border.color: CachyTheme.specularBorder
            }

            contentItem: RowLayout {
                spacing: CachyTheme.spacingSmall

                ColumnLayout {
                    spacing: 0
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.alignment: Qt.AlignVCenter

                    Text {
                        text: commandDelegate.model.name
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        elide: Text.ElideRight
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        color: CachyTheme.textPrimary
                    }
                    Text {
                        text: commandDelegate.model.command
                        font.italic: true
                        font.pixelSize: 11
                        color: CachyTheme.textMuted
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        elide: Text.ElideRight
                    }
                }

                CachyToolbarButton {
                    id: editButton
                    tooltipText: i18nd("kdeconnect-plugins", "Edit")
                    iconName: "edit-entry"
                    KeyNavigation.right: deleteButton
                    onClicked: {
                        editDialog.index = commandDelegate.model.index;
                        editNameField.text = commandDelegate.model.name;
                        editCommandField.text = commandDelegate.model.command;
                        editDialog.open();
                    }
                }

                CachyToolbarButton {
                    id: deleteButton
                    tooltipText: i18nd("kdeconnect-plugins", "Delete")
                    iconName: "edit-delete"
                    KeyNavigation.left: editButton
                    KeyNavigation.right: deleteButton
                    onClicked: commandModel.removeCommand(commandDelegate.model.index)
                }
            }
        }
    }

    CachyPlaceholderMessage {
        iconName: "utilities-terminal"
        anchors.centerIn: parent
        visible: view.count === 0
        width: parent.width - (CachyTheme.marginWindow * 2)
        text: i18ndc("kdeconnect-plugins", "@info", "No commands configured")
        helpfulAction: ({
            text: i18nd("kdeconnect-plugins", "Add Command…"),
            onTriggered: () => {
                nameField.text = ""
                commandField.text = ""
                addDialog.open()
            }
        })
    }

    CachyDialog {
        id: addDialog
        title: i18ndc("kdeconnect-plugins", "@title:window", "Add Command")
        preferredWidth: 420

        customFooterActions: [
            Kirigami.Action {
                text: i18ndc("kdeconnect-plugins", "@action:button", "Add")
                icon.name: "list-add"
                enabled: commandField.length > 0
                onTriggered: {
                    commandModel.addCommand(nameField.text, commandField.text);
                    addDialog.close();
                }
            }
        ]

        Text {
            text: i18nd("kdeconnect-plugins", "Name:")
            font.pixelSize: 12
            color: CachyTheme.textSecondary
        }

        CachyTextField {
            id: nameField
            Layout.fillWidth: true
        }

        Text {
            text: i18nd("kdeconnect-plugins", "Command:")
            font.pixelSize: 12
            color: CachyTheme.textSecondary
        }

        CachyTextField {
            id: commandField
            Layout.fillWidth: true
        }

        Text {
            text: i18nd("kdeconnect-plugins", "Sample commands:")
            font.pixelSize: 12
            color: CachyTheme.textSecondary
            visible: sampleCombo.visible
        }

        QQC2.ComboBox {
            id: sampleCombo
            Layout.fillWidth: true
            visible: model.length > 0
            textRole: "name"
            currentIndex: -1
            displayText: currentIndex === -1 ? "" : currentText
            model: createSampleCommands()
            onActivated: {
                if (currentIndex >= 0) {
                    nameField.text = model[currentIndex].name;
                    commandField.text = model[currentIndex].command;
                }
            }
            function createSampleCommands() {
                if (Qt.platform.os == "windows") {
                    return [
                        {
                            name: i18ndc("kdeconnect-plugins", "Sample command", "Suspend"),
                            command: "rundll32.exe powrprof.dll,SetSuspendState 0,1,0"
                        },
                        {
                            name: i18ndc("kdeconnect-plugins", "Sample command", "Lock Screen"),
                            command: "rundll32.exe user32.dll,LockWorkStation"
                        }
                    ];
                } else if (Qt.platform.os == "linux" || Qt.platform.os == "unix") {
                    return [
                        {
                            name: i18ndc("kdeconnect-plugins", "Sample command", "Suspend"),
                            command: "systemctl suspend"
                        },
                        {
                            name: i18ndc("kdeconnect-plugins", "Sample command", "Maximum Brightness"),
                            command: "qdbus org.kde.Solid.PowerManagement /org/kde/Solid/PowerManagement/Actions/BrightnessControl org.kde.Solid.PowerManagement.Actions.BrightnessControl.setBrightness `qdbus org.kde.Solid.PowerManagement /org/kde/Solid/PowerManagement/Actions/BrightnessControl org.kde.Solid.PowerManagement.Actions.BrightnessControl.brightnessMax`"
                        },
                        {
                            name: i18ndc("kdeconnect-plugins", "Sample command", "Lock Screen"),
                            command: "loginctl lock-session"
                        },
                        {
                            name: i18ndc("kdeconnect-plugins", "Sample command", "Unlock Screen"),
                            command: "loginctl unlock-session"
                        },
                        {
                            name: i18ndc("kdeconnect-plugins", "Sample command", "Close All Vaults"),
                            command: "qdbus org.kde.kded5 /modules/plasmavault closeAllVaults"
                        },
                        {
                            name: i18ndc("kdeconnect-plugins", "Sample command", "Forcefully Close All Vaults"),
                            command: "qdbus org.kde.kded5 /modules/plasmavault forceCloseAllVaults"
                        }
                    ];
                } else {
                    return [];
                }
            }
        }
    }

    CachyDialog {
        id: editDialog
        title: i18ndc("kdeconnect-plugins", "@title:window", "Edit Command")
        preferredWidth: 420
        property int index: 0

        customFooterActions: [
            Kirigami.Action {
                text: i18ndc("kdeconnect-plugins", "@action:button", "Confirm")
                icon.name: "dialog-ok"
                enabled: editCommandField.length > 0
                onTriggered: {
                    commandModel.changeCommand(editDialog.index, editNameField.text, editCommandField.text);
                    editDialog.close();
                }
            }
        ]

        Text {
            text: i18nd("kdeconnect-plugins", "Name:")
            font.pixelSize: 12
            color: CachyTheme.textSecondary
        }

        CachyTextField {
            id: editNameField
            Layout.fillWidth: true
        }

        Text {
            text: i18nd("kdeconnect-plugins", "Command:")
            font.pixelSize: 12
            color: CachyTheme.textSecondary
        }

        CachyTextField {
            id: editCommandField
            Layout.fillWidth: true
        }
    }
}
