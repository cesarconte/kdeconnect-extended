/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs as QtDialogs
import QtCore

import org.kde.kdeconnect as KDEConnect
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.extras as PlasmaExtras

Item {
    id: root

    required property KDEConnect.DevicesModel devicesModel

    implicitWidth: Kirigami.Units.gridUnit * 22
    implicitHeight: Kirigami.Units.gridUnit * 24

    // Configuración de visibilidad de proximidad
    // 0 = Nadie, 1 = Sólo conocidos, 2 = Todos (10 min)
    property int visibilityMode: 1
    property int countdownSeconds: 600

    Timer {
        id: countdownTimer
        interval: 1000
        repeat: true
        running: root.visibilityMode === 2 && root.countdownSeconds > 0
        onTriggered: {
            root.countdownSeconds -= 1
            if (root.countdownSeconds <= 0) {
                root.visibilityMode = 1; // Volver a sólo conocidos automáticamente
            }
        }
    }

    // Encabezado con estado del radar
    ColumnLayout {
        id: headerLayout
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: Kirigami.Units.smallSpacing
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter

            Kirigami.Icon {
                source: "network-wireless"
                implicitWidth: Kirigami.Units.iconSizes.small
                implicitHeight: Kirigami.Units.iconSizes.small
                color: Kirigami.Theme.highlightColor
            }

            PlasmaComponents3.Label {
                text: i18n("Radar de KDE Connect")
                font.bold: true
                font.pointSize: Kirigami.Theme.defaultFont.pointSize + 1
            }

            Item { Layout.fillWidth: true }

            PlasmaComponents3.Label {
                text: root.visibilityMode === 2 
                    ? i18n("Visible a todos (%1s)", root.countdownSeconds) 
                    : (root.visibilityMode === 1 ? i18n("Sólo conocidos") : i18n("No visible"))
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.8
            }
        }

        PlasmaComponents3.Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: i18n("Arrastra y suelta un archivo sobre cualquier dispositivo para compartirlo al instante.")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
        }
    }

    // Área central: Ondas de Radar y Nodos de Dispositivos
    Item {
        id: radarArea
        anchors.top: headerLayout.bottom
        anchors.bottom: controlsLayout.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Kirigami.Units.largeSpacing

        readonly property real centerX: width / 2
        readonly property real centerY: height / 2
        readonly property real maxRadius: Math.min(width, height) / 2 - Kirigami.Units.gridUnit * 2

        // Ondas concéntricas animadas
        Repeater {
            model: 3
            delegate: Rectangle {
                id: ring
                required property int index

                anchors.centerIn: parent
                width: 0
                height: width
                radius: width / 2
                color: "transparent"
                border.color: Kirigami.Theme.highlightColor
                border.width: 1.5
                opacity: 0.5

                SequentialAnimation {
                    running: true
                    loops: Animation.Infinite

                    PauseAnimation {
                        duration: ring.index * 1200
                    }

                    ParallelAnimation {
                        NumberAnimation {
                            target: ring
                            property: "width"
                            from: 40
                            to: radarArea.maxRadius * 2
                            duration: 3600
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: ring
                            property: "opacity"
                            from: 0.6
                            to: 0.0
                            duration: 3600
                            easing.type: Easing.OutQuad
                        }
                    }
                }
            }
        }

        // Anillos estáticos de referencia de distancia
        Rectangle {
            anchors.centerIn: parent
            width: radarArea.maxRadius * 1.3
            height: width
            radius: width / 2
            color: "transparent"
            border.color: Kirigami.Theme.textColor
            opacity: 0.08
            border.width: 1
        }

        Rectangle {
            anchors.centerIn: parent
            width: radarArea.maxRadius * 2
            height: width
            radius: width / 2
            color: "transparent"
            border.color: Kirigami.Theme.textColor
            opacity: 0.05
            border.width: 1
        }

        // Nodo Central: Este Equipo
        Rectangle {
            id: localHostNode
            anchors.centerIn: parent
            width: Kirigami.Units.iconSizes.large + Kirigami.Units.smallSpacing * 2
            height: width
            radius: width / 2
            color: Kirigami.Theme.backgroundColor
            border.color: Kirigami.Theme.highlightColor
            border.width: 2
            z: 10

            Kirigami.Icon {
                anchors.centerIn: parent
                source: "computer"
                implicitWidth: Kirigami.Units.iconSizes.medium
                implicitHeight: Kirigami.Units.iconSizes.medium
                color: Kirigami.Theme.highlightColor
            }

            Rectangle {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                width: Kirigami.Units.smallSpacing * 2.5
                height: width
                radius: width / 2
                color: Kirigami.Theme.positiveTextColor
                border.color: Kirigami.Theme.backgroundColor
                border.width: 1.5
            }

            PlasmaComponents3.ToolTip {
                text: i18n("Este equipo (Visible)")
            }
        }

        // Dispositivos detectados en órbita
        Repeater {
            id: devicesRepeater
            model: root.devicesModel

            delegate: Item {
                id: deviceNode
                required property var model
                required property int index

                readonly property int totalCount: devicesRepeater.count
                // Distribuir equitativamente en círculo
                readonly property real angle: totalCount > 0 ? (index * (2 * Math.PI / totalCount)) - (Math.PI / 2) : 0
                readonly property real orbitRadius: Math.max(radarArea.maxRadius * 0.72, Kirigami.Units.gridUnit * 5.5)

                x: radarArea.centerX + orbitRadius * Math.cos(angle) - width / 2
                y: radarArea.centerY + orbitRadius * Math.sin(angle) - height / 2

                width: Kirigami.Units.gridUnit * 5
                height: Kirigami.Units.gridUnit * 5.5

                readonly property string devId: model.Id ?? ""
                readonly property string devName: model.Name ?? i18n("Dispositivo")
                readonly property string devIcon: model.IconName ?? "smartphone"
                readonly property bool isReachable: model.IsReachable ?? true

                // Plugin Share asociado
                property KDEConnect.ShareDbusInterface sharePlugin: KDEConnect.ShareDbusInterfaceFactory.create(devId)

                QtDialogs.FileDialog {
                    id: deviceFileDialog
                    title: i18n("Enviar a %1", deviceNode.devName)
                    currentFolder: StandardPaths.standardLocations(StandardPaths.HomeLocation)[0]
                    modality: Qt.NonModal
                    fileMode: QtDialogs.FileDialog.OpenFiles
                    onAccepted: {
                        if (deviceNode.sharePlugin) {
                            deviceNode.sharePlugin.shareUrls(selectedFiles);
                        }
                    }
                }

                // Zona de Drag & Drop por dispositivo
                DropArea {
                    id: nodeDropArea
                    anchors.fill: parent

                    onEntered: {
                        avatarCircle.scale = 1.2
                    }

                    onExited: {
                        avatarCircle.scale = 1.0
                    }

                    onDropped: drop => {
                        avatarCircle.scale = 1.0
                        if (drop.hasUrls && deviceNode.sharePlugin) {
                            const urls = new Set(drop.urls.map(url => url.toString()));
                            urls.forEach(url => deviceNode.sharePlugin.shareUrl(url));
                            drop.accepted = true;
                        }
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 2

                    Rectangle {
                        id: avatarCircle
                        Layout.alignment: Qt.AlignHCenter
                        width: Kirigami.Units.iconSizes.large + 6
                        height: width
                        radius: width / 2
                        color: nodeDropArea.containsDrag ? Kirigami.Theme.highlightColor : Kirigami.Theme.cardBackgroundColor
                        border.color: nodeDropArea.containsDrag ? Kirigami.Theme.highlightColor : (deviceNode.isReachable ? Kirigami.Theme.highlightColor : Kirigami.Theme.disabledTextColor)
                        border.width: nodeDropArea.containsDrag ? 3 : 1.5

                        Behavior on scale {
                            NumberAnimation { duration: 180; easing.type: Easing.OutQuad }
                        }

                        Kirigami.Icon {
                            anchors.centerIn: parent
                            source: deviceNode.devIcon
                            implicitWidth: Kirigami.Units.iconSizes.medium
                            implicitHeight: Kirigami.Units.iconSizes.medium
                            color: nodeDropArea.containsDrag ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                        }

                        // Indicador de conexión
                        Rectangle {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            width: Kirigami.Units.smallSpacing * 2
                            height: width
                            radius: width / 2
                            color: deviceNode.isReachable ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.neutralTextColor
                            border.color: Kirigami.Theme.backgroundColor
                            border.width: 1
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                deviceFileDialog.open()
                            }
                        }
                    }

                    PlasmaComponents3.Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: deviceNode.devName
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }

                    PlasmaComponents3.Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: nodeDropArea.containsDrag ? i18n("¡Soltar aquí!") : (deviceNode.isReachable ? i18n("Conectado") : i18n("Fuera de alcance"))
                        font.pointSize: Math.max(7, Kirigami.Theme.smallFont.pointSize - 2)
                        color: nodeDropArea.containsDrag ? Kirigami.Theme.highlightColor : Kirigami.Theme.disabledTextColor
                    }
                }
            }
        }

        // Mensaje cuando no hay dispositivos en alcance
        PlasmaExtras.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width * 0.75
            visible: devicesRepeater.count === 0
            iconName: "network-wireless"
            text: i18n("Buscando dispositivos cercanos…")
            explanation: i18n("Asegúrate de que los otros equipos tengan KDE Connect activo en la misma red o Bluetooth.")
        }
    }

    // Controles inferiores: Selector de Visibilidad de proximidad
    ColumnLayout {
        id: controlsLayout
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Kirigami.Units.smallSpacing
        spacing: Kirigami.Units.smallSpacing

        PlasmaComponents3.Label {
            text: i18n("Visibilidad de KDE Connect:")
            font.bold: true
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            Layout.alignment: Qt.AlignHCenter
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Kirigami.Units.smallSpacing

            PlasmaComponents3.Button {
                text: i18n("Nadie")
                checkable: true
                checked: root.visibilityMode === 0
                icon.name: "dialog-cancel"
                onClicked: root.visibilityMode = 0
            }

            PlasmaComponents3.Button {
                text: i18n("Sólo conocidos")
                checkable: true
                checked: root.visibilityMode === 1
                icon.name: "user-identity"
                onClicked: root.visibilityMode = 1
            }

            PlasmaComponents3.Button {
                text: i18n("Todos (10 min)")
                checkable: true
                checked: root.visibilityMode === 2
                icon.name: "system-users"
                onClicked: {
                    root.visibilityMode = 2
                    root.countdownSeconds = 600
                }
            }
        }
    }
}
