/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Proximity radar view of the KDE Connect plasmoid, migrated to the CachyOS.UI
 * design system (Golden Gate / Liquid Glass).
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtCore

import org.kde.kdeconnect as KDEConnect
import org.kde.kirigami as Kirigami
import CachyOS.UI

ColumnLayout {
    id: root

    required property KDEConnect.DevicesModel devicesModel

    implicitWidth: 396
    implicitHeight: 432
    Layout.fillWidth: true
    Layout.fillHeight: true

    spacing: CachyTheme.spacingSmall

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
                root.visibilityMode = 1;
            }
        }
    }

    // Subtítulo explicativo con respuesta dinámica al arrastrar archivos
    Text {
        Layout.fillWidth: true
        Layout.topMargin: CachyTheme.spacingSmall
        horizontalAlignment: Text.AlignHCenter
        text: radarDropArea.containsDrag
            ? i18n("¡Suelta el archivo para enviarlo al dispositivo!")
            : i18n("Arrastra y suelta un archivo desde Dolphin o tu escritorio aquí.")
        font.pixelSize: 11
        font.bold: radarDropArea.containsDrag
        color: radarDropArea.containsDrag ? CachyTheme.accentColor : CachyTheme.textMuted
        wrapMode: Text.WordWrap
    }

    // Área central: Ondas de Radar y Nodos de Dispositivos
    Item {
        id: radarArea
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 252
        clip: true

        readonly property real centerX: width / 2
        readonly property real centerY: height / 2
        readonly property real maxRadius: Math.max(72, Math.min(width, height) / 2 - 27)

        DropArea {
            id: radarDropArea
            anchors.fill: parent
            z: 1

            onEntered: drag => {
                drag.acceptProposedAction();
            }

            onPositionChanged: drag => {
                drag.acceptProposedAction();
            }

            onDropped: drop => {
                if (drop.hasUrls && devicesRepeater.count > 0) {
                    const targetNode = devicesRepeater.itemAt(0);
                    if (targetNode && targetNode.sharePlugin) {
                        const urls = Array.from(new Set(drop.urls.map(url => url.toString())));
                        targetNode.sharePlugin.shareUrls(urls);
                        targetNode.notifySent();
                        drop.acceptProposedAction();
                        drop.accepted = true;
                        return;
                    }
                }
                drop.accepted = false;
            }
        }

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
                border.color: CachyTheme.accentColor
                border.width: radarDropArea.containsDrag ? 2.5 : 1.5
                opacity: 0.5

                SequentialAnimation {
                    running: true
                    loops: Animation.Infinite

                    PauseAnimation {
                        duration: ring.index * (radarDropArea.containsDrag ? 700 : 1200)
                    }

                    ParallelAnimation {
                        NumberAnimation {
                            target: ring
                            property: "width"
                            from: 40
                            to: radarArea.maxRadius * 2
                            duration: radarDropArea.containsDrag ? 2000 : 3600
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: ring
                            property: "opacity"
                            from: 0.7
                            to: 0.0
                            duration: radarDropArea.containsDrag ? 2000 : 3600
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
            border.color: CachyTheme.textPrimary
            opacity: 0.08
            border.width: 1
        }

        Rectangle {
            anchors.centerIn: parent
            width: radarArea.maxRadius * 2
            height: width
            radius: width / 2
            color: "transparent"
            border.color: CachyTheme.textPrimary
            opacity: 0.05
            border.width: 1
        }

        // Nodo Central: Este Equipo
        Rectangle {
            id: localHostNode
            anchors.centerIn: parent
            width: 56
            height: width
            radius: width / 2
            color: CachyTheme.cardBackground
            border.color: CachyTheme.accentColor
            border.width: 2
            z: 10

            Kirigami.Icon {
                anchors.centerIn: parent
                source: "computer"
                implicitWidth: 32
                implicitHeight: 32
                color: CachyTheme.accentColor
            }

            Rectangle {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                width: 12
                height: width
                radius: width / 2
                color: "#34C759"
                border.color: CachyTheme.cardBackground
                border.width: 1.5
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
                readonly property real angle: totalCount > 0 ? (index * (2 * Math.PI / totalCount)) - (Math.PI / 2) : 0
                readonly property real orbitRadius: Math.max(radarArea.maxRadius * 0.72, 90)

                x: radarArea.centerX + orbitRadius * Math.cos(angle) - width / 2
                y: radarArea.centerY + orbitRadius * Math.sin(angle) - height / 2

                width: 90
                height: 98
                z: 20

                readonly property string devId: model.deviceId ?? ""
                readonly property string devName: model.name ?? i18n("Dispositivo")
                readonly property string devIcon: model.iconName ?? "smartphone"
                readonly property bool isReachable: (model.device && model.device.isReachable) ?? true

                property bool justSent: false

                Timer {
                    id: sentTimer
                    interval: 2200
                    onTriggered: deviceNode.justSent = false
                }

                function notifySent() {
                    deviceNode.justSent = true;
                    sentTimer.restart();
                }

                property KDEConnect.ShareDbusInterface sharePlugin: devId ? KDEConnect.ShareDbusInterfaceFactory.create(devId) : null

                DropArea {
                    id: nodeDropArea
                    anchors.fill: parent
                    z: 50

                    onEntered: drag => {
                        avatarCircle.scale = 1.25;
                        drag.acceptProposedAction();
                    }

                    onPositionChanged: drag => {
                        drag.acceptProposedAction();
                    }

                    onExited: {
                        avatarCircle.scale = 1.0;
                    }

                    onDropped: drop => {
                        avatarCircle.scale = 1.0;
                        if (drop.hasUrls && deviceNode.sharePlugin) {
                            const urls = Array.from(new Set(drop.urls.map(url => url.toString())));
                            deviceNode.sharePlugin.shareUrls(urls);
                            deviceNode.notifySent();
                            drop.acceptProposedAction();
                            drop.accepted = true;
                        }
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 2
                    z: 10

                    Rectangle {
                        id: avatarCircle
                        Layout.alignment: Qt.AlignHCenter
                        width: 54
                        height: width
                        radius: width / 2
                        color: nodeDropArea.containsDrag
                            ? CachyTheme.accentColor
                            : (deviceNode.justSent ? "#34C759" : CachyTheme.cardBackground)
                        border.color: nodeDropArea.containsDrag
                            ? CachyTheme.accentColor
                            : (deviceNode.isReachable ? CachyTheme.accentColor : CachyTheme.textMuted)
                        border.width: nodeDropArea.containsDrag ? 3 : 1.5

                        Behavior on scale {
                            NumberAnimation { duration: 180; easing.type: Easing.OutQuad }
                        }

                        Kirigami.Icon {
                            anchors.centerIn: parent
                            source: deviceNode.justSent ? "dialog-ok" : deviceNode.devIcon
                            implicitWidth: 32
                            implicitHeight: 32
                            color: (nodeDropArea.containsDrag || deviceNode.justSent) ? "#FFFFFF" : CachyTheme.textPrimary
                        }

                        Rectangle {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            width: 10
                            height: width
                            radius: width / 2
                            color: deviceNode.isReachable ? "#34C759" : "#FF9500"
                            border.color: CachyTheme.cardBackground
                            border.width: 1
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: deviceNode.devName
                        font.pixelSize: 11
                        color: CachyTheme.textPrimary
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }

                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: deviceNode.justSent
                            ? i18n("¡Enviado!")
                            : (nodeDropArea.containsDrag ? i18n("¡Soltar aquí!") : (deviceNode.isReachable ? i18n("Conectado") : i18n("Fuera de alcance")))
                        font.pixelSize: 10
                        font.bold: nodeDropArea.containsDrag || deviceNode.justSent
                        color: deviceNode.justSent
                            ? "#34C759"
                            : (nodeDropArea.containsDrag ? CachyTheme.accentColor : CachyTheme.textMuted)
                    }
                }
            }
        }

        CachyPlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width * 0.75
            visible: devicesRepeater.count === 0
            iconName: "network-wireless"
            text: i18n("Buscando dispositivos cercanos…")
        }
    }

    // Controles inferiores: Selector de Visibilidad de proximidad
    ColumnLayout {
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignHCenter
        Layout.bottomMargin: CachyTheme.spacingSmall
        spacing: 4

        Text {
            text: root.visibilityMode === 2 && root.countdownSeconds > 0
                ? i18n("Permitir que me descubran: Todos (%1s)", root.countdownSeconds)
                : i18n("Permitir que me descubran:")
            font.bold: true
            font.pixelSize: 11
            color: CachyTheme.textMuted
            Layout.alignment: Qt.AlignHCenter
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: CachyTheme.spacingSmall

            CachyToolbarButton {
                Layout.fillWidth: true
                text: i18n("Nadie")
                iconName: "dialog-cancel"
                checkable: true
                checked: root.visibilityMode === 0
                onClicked: root.visibilityMode = 0
            }

            CachyToolbarButton {
                Layout.fillWidth: true
                text: i18n("Contactos")
                iconName: "user-identity"
                checkable: true
                checked: root.visibilityMode === 1
                onClicked: root.visibilityMode = 1
            }

            CachyToolbarButton {
                Layout.fillWidth: true
                text: i18n("Todos (10m)")
                iconName: "system-users"
                checkable: true
                checked: root.visibilityMode === 2
                onClicked: {
                    root.visibilityMode = 2
                    root.countdownSeconds = 600
                }
            }
        }
    }
}
