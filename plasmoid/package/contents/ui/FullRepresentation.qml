/**
 * SPDX-FileCopyrightText: 2013 Albert Vaca <albertvaka@gmail.com>
 * SPDX-FileCopyrightText: 2024 ivan tkachenko <me@ratijas.tk>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.config as KConfig
import org.kde.kdeconnect as KDEConnect
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.extras as PlasmaExtras

PlasmaExtras.Representation {
    id: kdeconnect

    property alias devicesModel: devicesView.model
    property bool isRadarView: true

    collapseMarginsHint: true

    KDEConnect.DevicesModel {
        id: allDevicesModel
    }

    KDEConnect.DevicesModel {
        id: pairedDevicesModel
        displayFilter: KDEConnect.DevicesModel.Paired
    }

    header: RowLayout {
        spacing: Kirigami.Units.smallSpacing

        PlasmaComponents3.Label {
            text: kdeconnect.isRadarView ? i18n("Radar de KDE Connect") : i18n("Dispositivos")
            font.bold: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.fillWidth: true
        }

        PlasmaComponents3.ToolButton {
            icon.name: kdeconnect.isRadarView ? "view-list-details" : "network-wireless"
            text: kdeconnect.isRadarView ? i18n("Ver lista") : i18n("Ver radar")
            display: PlasmaComponents3.AbstractButton.IconOnly
            PlasmaComponents3.ToolTip {
                text: kdeconnect.isRadarView ? i18n("Cambiar a vista de lista") : i18n("Cambiar a vista de radar")
            }
            onClicked: kdeconnect.isRadarView = !kdeconnect.isRadarView
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.smallSpacing

        HandoffBanner {
            id: handoffBanner
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing
            visible: hasActivity
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            DeviceRadar {
                anchors.fill: parent
                visible: kdeconnect.isRadarView
                devicesModel: pairedDevicesModel
            }

            PlasmaComponents3.ScrollView {
                anchors.fill: parent
                visible: !kdeconnect.isRadarView
                contentWidth: availableWidth
                
                // Prevent horizontal scrollbar from appearing when vertical scrollbar shows/hides
                QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

                contentItem: ListView {
                    id: devicesView

                    clip: true

                    Layout.margins: 0

                    delegate: DeviceDelegate {
                        width: ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin
                    }

                PlasmaExtras.PlaceholderMessage {
                    width: parent.width
                    anchors.centerIn: parent
                    visible: devicesView.count === 0

                    iconName: {
                        if (pairedDevicesModel.count >= 0) {
                            return pairedDevicesModel.count === 0 ? "edit-none" : "network-disconnect";
                        }
                        return "kdeconnect";
                    }

                    text: {
                        if (pairedDevicesModel.count >= 0) {
                            return pairedDevicesModel.count === 0 ? i18n("No paired devices") : i18np("Paired device is unavailable", "All paired devices are unavailable", pairedDevicesModel.count)
                        } else if (allDevicesModel.count === 0) {
                            return i18n("Install KDE Connect on your Android device to integrate it with Plasma!")
                        }
                    }
                    helpfulAction: QQC2.Action {
                        text: i18n("Pair a Device…")
                        icon.name: "list-add"
                        onTriggered: KDEConnect.OpenConfig.openConfiguration()
                        enabled: pairedDevicesModel.count === 0
                    }

                    PlasmaComponents3.Button {
                        Layout.leftMargin: Kirigami.Units.gridUnit * 3
                        Layout.rightMargin: Kirigami.Units.gridUnit * 3
                        Layout.alignment: Qt.AlignHCenter
                        Layout.fillWidth: true
                        visible: allDevicesModel.count === 0
                        text: i18n("Install from Google Play")
                        onClicked: Qt.openUrlExternally("https://play.google.com/store/apps/details?id=org.kde.kdeconnect_tp")
                    }

                    PlasmaComponents3.Button {
                        Layout.leftMargin: Kirigami.Units.gridUnit * 3
                        Layout.rightMargin: Kirigami.Units.gridUnit * 3
                        Layout.alignment: Qt.AlignHCenter
                        Layout.fillWidth: true
                        visible: allDevicesModel.count === 0
                        text: i18n("Install from F-Droid")
                        onClicked: Qt.openUrlExternally("https://f-droid.org/en/packages/org.kde.kdeconnect_tp/")
                    }
                }
            }
        }
    }
}
}
