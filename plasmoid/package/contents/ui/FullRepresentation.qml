/**
 * SPDX-FileCopyrightText: 2013 Albert Vaca <albertvaka@gmail.com>
 * SPDX-FileCopyrightText: 2024 ivan tkachenko <me@ratijas.tk>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Full representation of the KDE Connect plasmoid, migrated to the CachyOS.UI
 * design system (Golden Gate / Liquid Glass).
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.config as KConfig
import org.kde.kdeconnect as KDEConnect
import org.kde.kirigami as Kirigami
import org.kde.plasma.extras as PlasmaExtras
import org.kde.plasma.plasmoid
import CachyOS.UI

PlasmaExtras.Representation {
    id: kdeconnect

    property PlasmoidItem plasmoidItem
    property alias devicesModel: devicesView.model
    property bool isRadarView: true

    collapseMarginsHint: true

    implicitWidth: 396
    implicitHeight: 450

    Layout.minimumWidth: 342
    Layout.minimumHeight: 396
    Layout.preferredWidth: 396
    Layout.preferredHeight: 450

    Component.onCompleted: {
        if (kdeconnect.isRadarView && kdeconnect.plasmoidItem) {
            kdeconnect.plasmoidItem.isPinned = true;
        }
    }

    KDEConnect.DevicesModel {
        id: allDevicesModel
    }

    KDEConnect.DevicesModel {
        id: pairedDevicesModel
        displayFilter: KDEConnect.DevicesModel.Paired
    }

    header: RowLayout {
        spacing: CachyTheme.spacingSmall

        Kirigami.Icon {
            source: kdeconnect.isRadarView ? "network-wireless" : "kdeconnect"
            implicitWidth: 18
            implicitHeight: 18
            color: CachyTheme.accentColor
            Layout.leftMargin: CachyTheme.spacingSmall
        }

        CachyHeading {
            text: kdeconnect.isRadarView ? i18n("Radar de KDE Connect") : i18n("Dispositivos")
            level: 4
            Layout.fillWidth: true
            Layout.minimumWidth: 0
        }

        CachyToolbarButton {
            id: pinBtn
            iconName: "window-pin"
            checkable: true
            checked: kdeconnect.plasmoidItem ? kdeconnect.plasmoidItem.isPinned : true
            tooltipText: pinBtn.checked
                ? i18n("Fijada abierta (permite arrastrar archivos desde Dolphin sin que se cierre)")
                : i18n("Desanclada (cerrar al cambiar de ventana)")
            onToggled: (next) => {
                if (kdeconnect.plasmoidItem) {
                    kdeconnect.plasmoidItem.isPinned = next;
                }
            }
        }

        CachyToolbarButton {
            iconName: kdeconnect.isRadarView ? "view-list-details" : "network-wireless"
            tooltipText: kdeconnect.isRadarView ? i18n("Cambiar a vista de lista") : i18n("Cambiar a vista de radar")
            onClicked: {
                kdeconnect.isRadarView = !kdeconnect.isRadarView;
                if (kdeconnect.isRadarView && kdeconnect.plasmoidItem) {
                    kdeconnect.plasmoidItem.isPinned = true;
                }
            }
        }

        CachyToolbarButton {
            visible: Boolean(kdeconnect.plasmoidItem && kdeconnect.plasmoidItem.isPinned)
            iconName: "window-close"
            tooltipText: i18n("Cerrar ventana")
            onClicked: {
                if (kdeconnect.plasmoidItem) {
                    kdeconnect.plasmoidItem.expanded = false;
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: CachyTheme.spacingSmall

        HandoffBanner {
            id: handoffBanner
            Layout.fillWidth: true
            Layout.margins: CachyTheme.spacingSmall
            devicesModel: pairedDevicesModel
        }

        Item {
            id: contentContainer
            Layout.fillWidth: true
            Layout.fillHeight: true
            implicitWidth: kdeconnect.isRadarView ? deviceRadar.implicitWidth : devicesView.implicitWidth
            implicitHeight: kdeconnect.isRadarView ? deviceRadar.implicitHeight : 360

            DeviceRadar {
                id: deviceRadar
                anchors.fill: parent
                visible: kdeconnect.isRadarView
                devicesModel: pairedDevicesModel
            }

            QQC2.ScrollView {
                anchors.fill: parent
                visible: !kdeconnect.isRadarView
                contentWidth: availableWidth
                QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

                contentItem: ListView {
                    id: devicesView
                    clip: true
                    spacing: CachyTheme.spacingSmall

                    delegate: DeviceDelegate {
                        width: ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin
                    }
                }
            }

            CachyPlaceholderMessage {
                width: parent.width - (CachyTheme.marginWindow * 2)
                anchors.centerIn: parent
                visible: !kdeconnect.isRadarView && devicesView.count === 0

                iconName: {
                    if (pairedDevicesModel.count >= 0) {
                        return pairedDevicesModel.count === 0 ? "edit-none-symbolic" : "network-disconnect-symbolic";
                    }
                    return "kdeconnect";
                }

                text: {
                    if (pairedDevicesModel.count >= 0) {
                        return pairedDevicesModel.count === 0
                            ? i18n("No paired devices")
                            : i18np("Paired device is unavailable", "All paired devices are unavailable", pairedDevicesModel.count);
                    } else if (allDevicesModel.count === 0) {
                        return i18n("Install KDE Connect on your Android device to integrate it with Plasma!");
                    }
                    return "";
                }

                helpfulAction: pairedDevicesModel.count === 0 ? ({
                    text: i18n("Pair a Device…"),
                    onTriggered: () => KDEConnect.OpenConfig.openConfiguration()
                }) : null

                CachyButton {
                    Layout.alignment: Qt.AlignHCenter
                    visible: allDevicesModel.count === 0
                    text: i18n("Install from Google Play")
                    onClicked: Qt.openUrlExternally("https://play.google.com/store/apps/details?id=org.kde.kdeconnect_tp")
                }

                CachyButton {
                    Layout.alignment: Qt.AlignHCenter
                    visible: allDevicesModel.count === 0
                    text: i18n("Install from F-Droid")
                    onClicked: Qt.openUrlExternally("https://f-droid.org/en/packages/org.kde.kdeconnect_tp/")
                }
            }
        }
    }
}
