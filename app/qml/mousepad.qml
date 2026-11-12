/*
 * SPDX-FileCopyrightText: 2015 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Remote touchpad and keyboard page of the KDE Connect application, migrated
 * to the CachyOS.UI design system (Golden Gate / Liquid Glass).
 */

import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kdeconnect
import CachyOS.UI

Item {
    id: mousepad

    property string title: i18nd("kdeconnect-app", "Remote Control")
    property QtObject pluginInterface
    property QtObject device

    Component.onCompleted: {
        PointerLocker.window = Window.window
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: CachyTheme.spacingMedium

        CachyCard {
            Layout.fillWidth: true
            Layout.fillHeight: true

            MouseArea {
                id: area
                anchors.fill: parent
                property var lastPos: Qt.point(-1, -1)
                property var pressedPos: Qt.point(-1, -1)
                property var releasedPos: Qt.point(-1, -1)

                acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton

                CachyButton {
                    id: lockButton
                    anchors.centerIn: parent
                    text: i18n("Lock")
                    focus: true
                    visible: !PointerLocker.isLocked
                    KeyNavigation.down: middleClickButton
                    onClicked: {
                        PointerLocker.isLocked = true
                        area.pressedPos = Qt.point(-1, -1);
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: PointerLocker.isLocked
                    width: parent.width - (CachyTheme.marginCard * 2)

                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: 12
                    color: CachyTheme.textSecondary

                    text: i18n("Press %1 or the left and right mouse buttons at the same time to unlock", unlockShortcut.nativeText)
                    onVisibleChanged: {
                        if (visible && Accessible.announce) {
                            Accessible.announce(text);
                        }
                    }
                }

                Connections {
                    target: PointerLocker
                    function onPointerMoved(delta) {
                        if (!PointerLocker.isLocked) {
                            return;
                        }
                        mousepad.pluginInterface.moveCursor(Qt.point(delta.x, delta.y));
                    }
                }

                // We don't want to see the lock button when using a touchscreen
                TapHandler {
                    acceptedDevices: PointerDevice.TouchScreen
                    onTapped: lockButton.visible = false
                }

                Shortcut {
                    id: unlockShortcut
                    sequence: "Alt+X"
                    onActivated: {
                        PointerLocker.isLocked = false
                    }
                }

                onClicked: (mouse) => {
                    var clickType = "";
                    var packet = {};
                    switch (mouse.button) {
                    case Qt.LeftButton:
                        if (PointerLocker.isLocked)
                            return;
                        if (pressedPos == releasedPos) {
                            clickType = "singleclick";
                        }
                        break;
                    case Qt.RightButton:
                        clickType = "rightclick";
                        break;
                    case Qt.MiddleButton:
                        clickType = "middleclick";
                        break;
                    default:
                        break;
                    }

                    if (clickType) {
                        packet[clickType] = true;
                        mousepad.pluginInterface.sendCommand(packet);
                    }
                }

                onPressAndHold: (mouse) => {
                    if (PointerLocker.isLocked)
                        return;
                    var clickType = "";
                    var packet = {};
                    switch (mouse.button) {
                    case Qt.LeftButton:
                        clickType = "singlehold";
                        break;
                    default:
                        break;
                    }

                    if (clickType) {
                        packet[clickType] = true;
                        mousepad.pluginInterface.sendCommand(packet);
                    }
                }

                onPositionChanged: (mouse) => {
                    if (lastPos.x > -1) {
                        var delta = Qt.point(mouse.x-lastPos.x, mouse.y-lastPos.y);
                        pluginInterface.moveCursor(delta);
                    }
                    lastPos = Qt.point(mouse.x, mouse.y);
                }

                Keys.onPressed: (event) => {
                    if (event.key == Qt.Key_X) {
                        PointerLocker.isLocked = false
                        event.accepted = true;
                    }
                }

                onPressed: (mouse) => {
                    if (PointerLocker.isLocked) {
                        if (pressedButtons === (Qt.LeftButton | Qt.RightButton)) {
                            PointerLocker.isLocked = false
                        }
                        var pressType = "";
                        var packet = {};
                        switch (mouse.buttons) {
                        case Qt.LeftButton:
                            pressType = "singlehold";
                            break;
                        default:
                            break;
                        }
                        if (pressType) {
                            packet[pressType] = true;
                            mousepad.pluginInterface.sendCommand(packet);
                        }
                    } else {
                        pressedPos = Qt.point(mouse.x, mouse.y);
                    }
                }

                onWheel: (wheel) => {
                    var packet = {};
                    packet["scroll"] = true;
                    packet["dy"] = wheel.angleDelta.y;
                    packet["dx"] = wheel.angleDelta.x;
                    mousepad.pluginInterface.sendCommand(packet);
                }

                onReleased: (mouse) => {
                    if (!PointerLocker.isLocked) {
                        lastPos = Qt.point(-1,-1);
                        releasedPos = Qt.point(mouse.x, mouse.y);
                    }
                    mousepad.pluginInterface.sendCommand({"singlerelease" : true});
                }
            }
        }

        RemoteKeyboard {
            device: mousepad.device
            Layout.fillWidth: true
            visible: remoteState
            focus: !lockButton.visible
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: CachyTheme.spacingSmall

            CachyButton {
                id: leftClickButton
                Layout.fillWidth: true
                iconName: "input-mouse-click-left"
                Accessible.name: i18nc("@action:button accessible", "Left Click")
                KeyNavigation.right: middleClickButton
                KeyNavigation.up: lockButton
                onClicked: mousepad.pluginInterface.sendCommand({"singleclick": true});
            }
            CachyButton {
                id: middleClickButton
                Layout.fillWidth: true
                iconName: "input-mouse-click-middle"
                Accessible.name: i18nc("@action:button accessible", "Middle Click")
                KeyNavigation.right: rightClickButton
                KeyNavigation.up: lockButton
                onClicked: mousepad.pluginInterface.sendCommand({"middleclick": true});
            }
            CachyButton {
                id: rightClickButton
                Layout.fillWidth: true
                iconName: "input-mouse-click-right"
                Accessible.name: i18nc("@action:button accessible", "Right Click")
                KeyNavigation.up: lockButton
                onClicked: mousepad.pluginInterface.sendCommand({"rightclick": true});
            }
        }
    }
}
