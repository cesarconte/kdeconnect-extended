/**
 * SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo.de>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Share-input-devices plugin configuration page, migrated to the CachyOS.UI
 * design system (Golden Gate / Liquid Glass).
 */

import QtQml.Models
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kdeconnect
import org.kde.kitemmodels as KItemModels
import org.kde.kirigami as Kirigami
import CachyOS.UI

CachyFormCardPage {
    id: root

    property string device
    readonly property string pluginId: "kdeconnect_shareinputdevices"

    readonly property list<Kirigami.Action> headerActions: [
        Kirigami.Action {
            id: applyButton
            icon.name: "dialog-ok"
            text: i18nd("kdeconnect-plugins", "Apply")
            enabled: false
            onTriggered: {
                config.set("edge", edgeComboBox.valueAt(edgeComboBox.currentIndex))
                enabled = false
            }
        }
    ]

    KdeConnectPluginConfig {
        id: config
        deviceId: root.device
        pluginName: root.pluginId

        onConfigChanged: edgeComboBox.currentIndex = edgeComboBox.indexOfValue(getInt("edge", Qt.LeftEdge))
    }

    Instantiator {
        id: configInstantiator
        readonly property var children: Array.from({length: configInstantiator.count}, (value, index) => configInstantiator.objectAt(index))
        delegate: KdeConnectPluginConfig {
            required property var model
            deviceId: model.deviceId
            pluginName: root.pluginId
        }
        model: KItemModels.KSortFilterProxyModel {
            sourceModel: DevicesModel {}
            filterRowCallback: function(source_row, source_parent) {
                const dev = sourceModel.getDevice(source_row)
                return dev.id() !== root.device && dev.isPluginEnabled(root.pluginId);
            }
        }
    }

    CachyFormCard {
        Layout.fillWidth: true

        RowLayout {
            Layout.fillWidth: true
            spacing: CachyTheme.spacingMedium

            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: i18nc("@label:listbox Forms a sentence: 'Leave the screen at the top/bottom/left/right edge', the pointer will switch to the connected device there", "Leave Screen at")
                font.pixelSize: 12
                color: CachyTheme.textPrimary
            }

            QQC2.ComboBox {
                id: edgeComboBox
                textRole: "text"
                valueRole: "value"
                onActivated: applyButton.enabled = edgeComboBox.valueAt(edgeComboBox.currentIndex) != config.getInt("edge", Qt.LeftEdge)
                model: [
                    {
                        text: i18nc("@item:inlistbox top edge of screen", "Top"),
                        value: Qt.TopEdge
                    },
                    {
                        text: i18nc("@item:inlistbox left edge of screen", "Left"),
                        value: Qt.LeftEdge
                    },
                    {
                        text: i18nc("@item:inlistbox right edge of screen", "Right"),
                        value: Qt.RightEdge
                    },
                    {
                        text: i18nc("@item:inlistbox top edge of screen", "Bottom"),
                        value: Qt.BottomEdge
                    }
                ]
                Component.onCompleted: currentIndex = edgeComboBox.indexOfValue(config.getInt("edge", Qt.LeftEdge))
            }
        }

        CachyInlineMessage {
            Layout.fillWidth: true
            visible: configInstantiator.children.some((deviceConfig) => deviceConfig && deviceConfig.getInt("edge", Qt.LeftEdge) == edgeComboBox.currentValue)
            messageType: "warning"
            iconName: "data-warning"
            text: i18nc("@info:tooltip edge: screen edge", "Another device already reserved this edge. This may lead to unexpected results when both are connected at the same time.")
        }
    }
}
