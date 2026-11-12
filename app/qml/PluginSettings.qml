/*
 * SPDX-FileCopyrightText: 2019 Nicolas Fella <nicolas.fella@gmx.de>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Plugin settings page of the KDE Connect application, migrated to the
 * CachyOS.UI design system (Golden Gate / Liquid Glass).
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kitemmodels as KItemModels
import org.kde.kdeconnect
import CachyOS.UI

CachyScrollablePage {
    id: root

    property string device
    property string filterString
    readonly property bool isPluginSettingsPage: true

    /// The stack of pages of the window.
    property var pageStack: null

    title: i18n("Plugin Settings")

    function openConfiguration(pluginName) {
        let source = pluginModel.pluginSource(pluginName);
        if (!source || String(source).length === 0) {
            return null;
        }
        return pageStack.push(source, {
            title: pluginModel.pluginDisplayName(pluginName),
            device: root.device,
        });
    }

    header: Item {
        implicitHeight: searchField.implicitHeight + 16

        CachySearchField {
            id: searchField
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 8
            onTextChanged: root.filterString = text;
            focus: true
            Keys.onDownPressed: event => {
                pluginList.currentIndex = 0;
                event.accepted = false; // Pass to KeyNavigation.down
            }
            KeyNavigation.down: pluginList
        }
    }

    ListView {
        id: pluginList
        Accessible.role: Accessible.List
        model: KItemModels.KSortFilterProxyModel {
            filterString: root.filterString
            filterRoleName: "name"
            filterCaseSensitivity: Qt.CaseInsensitive

            sourceModel: PluginModel {
                id: pluginModel
                deviceId: device
            }
        }

        delegate: QQC2.ItemDelegate {
            id: pluginDelegate

            width: ListView.view.width
            height: 56

            required property var model

            checkable: true
            checked: model.isChecked

            KeyNavigation.tab: settingsButton
            KeyNavigation.right: settingsButton

            Accessible.description: model.description
            Accessible.role: Accessible.CheckBox

            onClicked: {
                pluginList.currentIndex = model.index
                model.isChecked = !model.isChecked
            }

            contentItem: RowLayout {
                spacing: CachyTheme.spacingMedium

                CachySwitch {
                    Layout.alignment: Qt.AlignVCenter
                    checked: pluginDelegate.model.isChecked
                    // The state belongs to the model: the switch only shows it.
                    interactive: false
                }

                Image {
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    Layout.alignment: Qt.AlignVCenter
                    source: CachyTheme.iconUrl(pluginDelegate.model.iconName, "application-x-executable-symbolic")
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }

                ColumnLayout {
                    spacing: 0
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.alignment: Qt.AlignVCenter

                    Text {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: pluginDelegate.model.name
                        elide: Text.ElideRight
                        font.pixelSize: 13
                        color: CachyTheme.textPrimary
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: pluginDelegate.model.description
                        elide: Text.ElideRight
                        font.pixelSize: 11
                        color: CachyTheme.textMuted
                    }
                }

                CachyToolbarButton {
                    id: settingsButton

                    Layout.alignment: Qt.AlignVCenter
                    visible: pluginDelegate.model.configSource != ""

                    iconName: "settings-configure-symbolic"
                    tooltipText: i18n("Configure")

                    onClicked: {
                        pageStack.push(pluginDelegate.model.configSource, {
                            title: pluginDelegate.model.name,
                            device: root.device,
                        });
                    }
                }
            }

            background: Rectangle {
                radius: CachyTheme.radiusSmall
                color: pluginDelegate.hovered ? CachyTheme.cardBackgroundHover : "transparent"
            }
        }
    }
}
