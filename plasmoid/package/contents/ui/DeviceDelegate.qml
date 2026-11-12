/**
 * SPDX-FileCopyrightText: 2013 Albert Vaca <albertvaka@gmail.com>
 * SPDX-FileCopyrightText: 2024 ivan tkachenko <me@ratijas.tk>
 * SPDX-FileCopyrightText: 2026 Lito Parra <lito.15@proton.me>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Device delegate of the KDE Connect plasmoid, migrated to the CachyOS.UI
 * design system (Golden Gate / Liquid Glass).
 */

pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs as QtDialogs
import QtQuick.Layouts

import org.kde.kdeconnect as KDEConnect
import org.kde.kirigami as Kirigami
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.extras as PlasmaExtras
import CachyOS.UI

QQC2.ItemDelegate {
    id: root

    required property int index
    required property var model

    readonly property KDEConnect.DeviceDbusInterface device: KDEConnect.DeviceDbusInterfaceFactory.create(model.deviceId)

    property bool notificationsExpanded: false

    hoverEnabled: true
    down: false

    background: Rectangle {
        radius: CachyTheme.radiusMedium
        color: CachyTheme.cardBackground
        border.width: 1
        border.color: CachyTheme.specularBorder
    }

    padding: CachyTheme.spacingSmall

    Battery {
        id: battery
        device: root.device
    }

    Clipboard {
        id: clipboard
        device: root.device
    }

    Connectivity {
        id: connectivity
        device: root.device
    }

    FindMyPhone {
        id: findmyphone
        device: root.device
    }

    RemoteCommands {
        id: remoteCommands
        device: root.device
    }

    Sftp {
        id: sftp
        device: root.device
    }

    Share {
        id: share
        device: root.device
    }

    SMS {
        id: sms
        device: root.device
    }

    VirtualMonitor {
        id: virtualmonitor
        device: root.device
    }

    CachyDialog {
        id: prompt
        title: i18n("Virtual Monitor is not available")
    }

    QtDialogs.FileDialog {
        id: fileDialog
        title: i18n("Please choose a file")
        currentFolder: StandardPaths.standardLocations(StandardPaths.HomeLocation)[0]
        modality: Qt.NonModal
        fileMode: QtDialogs.FileDialog.OpenFiles
        onAccepted: share.plugin.shareUrls(fileDialog.selectedFiles)
    }

    PlasmaExtras.Menu {
        id: menu

        visualParent: overflowMenu
        placement: PlasmaExtras.Menu.BottomPosedLeftAlignedPopup

        PlasmaExtras.MenuItem {
            icon: "document-share"
            visible: share.available
            text: i18n("Share file")
            onClicked: fileDialog.open()
        }

        PlasmaExtras.MenuItem {
            icon: "klipper"
            visible: clipboard.clipboard?.isAutoShareDisabled ?? false
            text: i18n("Send Clipboard")
            onClicked: clipboard.sendClipboard()
        }

        PlasmaExtras.MenuItem {
            icon: "irc-voice"
            visible: findmyphone.available
            text: i18n("Ring my phone")
            onClicked: findmyphone.ring()
        }

        PlasmaExtras.MenuItem {
            icon: "document-open-folder"
            visible: sftp.available
            text: i18n("Browse this device")
            onClicked: sftp.browse()
        }

        PlasmaExtras.MenuItem {
            icon: "message-new"
            visible: sms.available
            text: i18n("SMS Messages")
            onClicked: sms.plugin.launchApp()
        }
    }

    DropArea {
        id: fileDropArea
        anchors.fill: parent

        onDropped: drop => {
            if (drop.hasUrls) {
                const urls = new Set(drop.urls.map(url => url.toString()));
                urls.forEach(url => share.plugin.shareUrl(url));
            }
            drop.accepted = true;
        }
    }

    contentItem: ColumnLayout {
        spacing: CachyTheme.spacingSmall

        QQC2.ItemDelegate {
            id: deviceHeaderContainer
            hoverEnabled: true
            padding: 0
            topPadding: CachyTheme.spacingSmall
            bottomPadding: topPadding

            Layout.fillWidth: true
            Layout.margins: 0

            background: Item {}

            PlasmaCore.ToolTipArea {
                anchors.fill: parent
                z: -1
                active: deviceHeaderContainer.hovered && !overflowMenu.isHovered && !virtualMonitorButton.isHovered && !dismissAllButton.isHovered
                mainText: i18n("File Transfer")
                subText: i18n("Drop a file to transfer it onto your phone.")
            }

            contentItem: ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignVCenter
                spacing: CachyTheme.spacingSmall
                Layout.margins: 0

                RowLayout {
                    spacing: CachyTheme.spacingSmall

                    CachyHeading {
                        text: root.model.name
                        level: 3
                        elide: Text.ElideRight
                        maximumLineCount: 1
                        leftPadding: LayoutMirroring.enabled ? 0 : CachyTheme.spacingSmall
                        rightPadding: LayoutMirroring.enabled ? CachyTheme.spacingSmall : 0
                    }

                    Item {
                        Layout.fillWidth: true
                        enabled: false
                    }

                    CachyToolbarButton {
                        id: virtualMonitorButton
                        iconName: "krdc"
                        visible: virtualmonitor.available
                        checked: visible && !!virtualmonitor.plugin?.active
                        checkable: true
                        tooltipText: i18n("Virtual Display")

                        onClicked: {
                            if (virtualmonitor.plugin.active) {
                                virtualmonitor.plugin.stop();
                                prompt.close();
                            } else {
                                virtualmonitor.plugin.requestVirtualMonitor();
                                prompt.subtitle = virtualmonitor.plugin.lastError;
                                if (prompt.subtitle.length > 0) {
                                    prompt.open();
                                }
                            }
                        }
                    }

                    RowLayout {
                        id: connectionInformation
                        visible: connectivity.available
                        spacing: CachyTheme.spacingTiny

                        Kirigami.Icon {
                            id: celluarConnectionStrengthIcon
                            source: connectivity.iconName
                            Layout.preferredHeight: 16
                            Layout.preferredWidth: 16
                            Layout.alignment: Qt.AlignCenter
                            visible: valid
                        }

                        Text {
                            id: connectivityText
                            text: connectivity.displayString
                            textFormat: Text.PlainText
                            font.pixelSize: 11
                            color: CachyTheme.textSecondary
                            visible: !celluarConnectionStrengthIcon.visible
                        }
                    }

                    RowLayout {
                        id: batteryInformation
                        visible: battery.available && battery.hasBattery
                        spacing: CachyTheme.spacingTiny

                        Kirigami.Icon {
                            id: batteryIcon
                            source: battery.iconName
                            Layout.preferredHeight: 16
                            Layout.preferredWidth: 16
                            Layout.alignment: Qt.AlignCenter
                        }

                        Text {
                            id: batteryPercent
                            text: i18nc("Battery charge percentage", "%1%", battery.charge)
                            textFormat: Text.PlainText
                            font.pixelSize: 11
                            color: CachyTheme.textSecondary
                        }
                    }

                    CachyToolbarButton {
                        id: overflowMenu
                        iconName: "application-menu"
                        tooltipText: i18n("More options…")
                        onClicked: menu.openRelative()
                    }
                }

                // Notifications Header
                RowLayout {
                    visible: notificationsModel.count > 0
                    Layout.fillWidth: true
                    Layout.margins: 0
                    spacing: CachyTheme.spacingSmall

                    TapHandler {
                        onTapped: root.notificationsExpanded = !root.notificationsExpanded
                        cursorShape: Qt.PointingHandCursor
                    }

                    Kirigami.Icon {
                        id: collapseIcon
                        source: LayoutMirroring.enabled ? "arrow-left" : "arrow-right"
                        Layout.preferredHeight: 16
                        Layout.preferredWidth: 16
                        color: CachyTheme.textPrimary

                        rotation: root.notificationsExpanded ? (LayoutMirroring.enabled ? -90 : 90) : 0

                        Behavior on rotation {
                            NumberAnimation {
                                duration: CachyTheme.animFast
                                easing.type: Easing.InOutQuad
                            }
                        }
                    }

                    Text {
                        text: i18n("Notifications")
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        color: CachyTheme.textPrimary
                        Layout.fillWidth: true
                    }

                    CachyToolbarButton {
                        id: dismissAllButton
                        enabled: true
                        visible: notificationsModel.isAnyDismissable
                        iconName: "edit-clear-history"
                        tooltipText: i18n("Dismiss all notifications")
                        onClicked: dismissAllAnimation.start()
                    }
                }
            }
        }

        // RemoteKeyboard
        RowLayout {
            visible: remoteKeyboard.remoteState
            Layout.fillWidth: true
            spacing: CachyTheme.spacingSmall

            Text {
                id: remoteKeyboardLabel
                text: i18n("Remote Keyboard")
                font.pixelSize: 12
                color: CachyTheme.textSecondary
            }

            KDEConnect.RemoteKeyboard {
                id: remoteKeyboard
                device: root.device
                Layout.fillWidth: true
            }
        }

        Item {
            id: notificationsContainer
            Layout.fillWidth: true
            Layout.topMargin: CachyTheme.spacingSmall
            Layout.preferredHeight: root.notificationsExpanded ? notificationsColumn.implicitHeight : 0
            clip: true
            visible: notificationsModel.count > 0

            Behavior on Layout.preferredHeight {
                NumberAnimation {
                    duration: CachyTheme.animFast
                    easing.type: Easing.InOutQuad
                }
            }

            SequentialAnimation {
                id: dismissAllAnimation
                NumberAnimation {
                    target: notificationsColumn
                    property: "x"
                    to: notificationsContainer.LayoutMirroring.enabled ? -notificationsContainer.width : notificationsContainer.width
                    duration: CachyTheme.animFast
                }
                ScriptAction {
                    script: {
                        notificationsModel.dismissAll();
                    }
                }
                PauseAnimation {
                    duration: 50
                }
                ScriptAction {
                    script: {
                        if (notificationsModel.count === 0) {
                            notificationsColumn.x = 0;
                        }
                    }
                }
            }

            Column {
                id: notificationsColumn
                width: parent.width

                move: Transition {
                    NumberAnimation {
                        properties: "y"
                        duration: CachyTheme.animFast
                        easing.type: Easing.OutInQuad
                    }
                }

                add: Transition {
                    NumberAnimation {
                        properties: "opacity"
                        from: 0
                        to: 1
                        duration: CachyTheme.animFast
                    }
                }

                Repeater {
                    id: notificationsView

                    model: KDEConnect.NotificationsModel {
                        id: notificationsModel
                        deviceId: root.model.deviceId
                    }

                    delegate: QQC2.ItemDelegate {
                        id: listitem

                        required property int index
                        required property var model

                        hoverEnabled: true
                        width: parent.width
                        height: implicitHeight
                        clip: true

                        property bool _suppressAnimations: false

                        topPadding: index != 0 ? CachyTheme.spacingMedium : 0
                        bottomPadding: index != notificationsModel.count - 1 ? CachyTheme.spacingMedium : 0
                        leftPadding: LayoutMirroring.enabled ? 0 : CachyTheme.spacingSmall
                        rightPadding: LayoutMirroring.enabled ? CachyTheme.spacingSmall : 0
                        Layout.margins: 0

                        SequentialAnimation {
                            id: dismissAnimation
                            NumberAnimation {
                                target: listitem
                                property: "x"
                                to: listitem.LayoutMirroring.enabled ? -listitem.width : listitem.width
                                duration: CachyTheme.animFast
                            }
                            ScriptAction {
                                script: listitem.model.dbusInterface.dismiss()
                            }
                        }

                        Behavior on height {
                            enabled: !listitem._suppressAnimations
                            NumberAnimation {
                                duration: CachyTheme.animFast
                                easing.type: Easing.InOutQuad
                            }
                        }

                        background: Item {}

                        enabled: true
                        onClicked: checked = !checked

                        Timer {
                            id: collapseDelayTimer
                            interval: CachyTheme.animFast
                            onTriggered: {
                                listitem._suppressAnimations = true
                                listitem.checked = false
                                listitem._suppressAnimations = false
                            }
                        }

                        Connections {
                            target: root
                            function onNotificationsExpandedChanged() {
                                if (!root.notificationsExpanded) {
                                    collapseDelayTimer.start()
                                } else {
                                    collapseDelayTimer.stop()
                                }
                            }
                        }

                        property bool replying: false

                        contentItem: ColumnLayout {
                            spacing: CachyTheme.spacingSmall

                            RowLayout {
                                spacing: CachyTheme.spacingMedium
                                Layout.alignment: Qt.AlignTop

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    Layout.alignment: Qt.AlignTop

                                    RowLayout {
                                        id: notificationHead
                                        spacing: CachyTheme.spacingMedium
                                        Layout.alignment: Qt.AlignTop

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            spacing: CachyTheme.spacingTiny

                                            RowLayout {
                                                Layout.alignment: Qt.AlignVCenter
                                                spacing: CachyTheme.spacingSmall

                                                Kirigami.Icon {
                                                    id: notificationIcon
                                                    source: listitem.model.appIcon
                                                    Layout.preferredWidth: 18
                                                    Layout.preferredHeight: 18
                                                    Layout.alignment: Qt.AlignVCenter
                                                }

                                                CachyHeading {
                                                    id: notificationAppName
                                                    text: listitem.model.appName
                                                    textFormat: Text.PlainText
                                                    level: 5
                                                    elide: listitem.checked ? Text.ElideNone : Text.ElideRight
                                                    maximumLineCount: listitem.checked ? 0 : 1
                                                    wrapMode: Text.Wrap
                                                    Layout.alignment: Qt.AlignVCenter
                                                    Layout.fillWidth: true
                                                    Layout.minimumWidth: 0
                                                    color: CachyTheme.textSecondary
                                                }
                                            }

                                            CachyHeading {
                                                id: notificationTitle
                                                text: listitem.model.title
                                                textFormat: Text.PlainText
                                                level: 3
                                                visible: text.length > 0
                                                elide: listitem.checked ? Text.ElideNone : Text.ElideRight
                                                maximumLineCount: listitem.checked ? 0 : 1
                                                wrapMode: Text.Wrap
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                            }
                                        }

                                        CachyToolbarButton {
                                            id: replyButton
                                            visible: listitem.model.repliable
                                            enabled: listitem.model.repliable && !listitem.replying
                                            iconName: "mail-reply-sender"
                                            tooltipText: i18n("Reply")
                                            onClicked: {
                                                listitem.replying = true;
                                                replyTextField.forceActiveFocus();
                                            }
                                            Layout.alignment: Qt.AlignTop
                                        }

                                        CachyToolbarButton {
                                            id: dismissButton
                                            visible: notificationsModel.isAnyDismissable
                                            enabled: listitem.model.dismissable
                                            Layout.alignment: Qt.AlignTop
                                            iconName: "window-close"
                                            tooltipText: i18n("Dismiss")
                                            onClicked: dismissAnimation.start()
                                        }
                                    }

                                    Text {
                                        id: notificationNotitext
                                        text: listitem.model.notitext
                                        visible: text.length > 0
                                        elide: listitem.checked ? Text.ElideNone : Text.ElideRight
                                        maximumLineCount: listitem.checked ? 0 : 1
                                        wrapMode: Text.Wrap
                                        font.pixelSize: 12
                                        color: CachyTheme.textPrimary
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                    }
                                }
                            }

                            RowLayout {
                                visible: listitem.replying
                                spacing: CachyTheme.spacingSmall
                                Layout.topMargin: CachyTheme.spacingSmall
                                Layout.fillWidth: true

                                CachyToolbarButton {
                                    id: replyCancelButton
                                    Layout.alignment: Qt.AlignBottom
                                    tooltipText: i18n("Cancel")
                                    iconName: "dialog-cancel"
                                    onClicked: {
                                        replyTextField.text = "";
                                        listitem.replying = false;
                                    }
                                }

                                CachyTextField {
                                    id: replyTextField
                                    placeholderText: i18nc("@info:placeholder", "Reply to %1…", listitem.model.appName)
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    Keys.onPressed: event => {
                                        if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && !(event.modifiers & Qt.ShiftModifier)) {
                                            if (replyTextField.text !== "") {
                                                replySendButton.clicked();
                                            }
                                            event.accepted = true;
                                        }
                                        if (event.key === Qt.Key_Escape) {
                                            replyCancelButton.clicked();
                                            event.accepted = true;
                                        }
                                    }
                                }

                                CachyToolbarButton {
                                    Layout.alignment: Qt.AlignBottom
                                    id: replySendButton
                                    tooltipText: i18n("Send")
                                    iconName: LayoutMirroring.enabled ? "document-send-rtl" : "document-send"
                                    primary: true
                                    enabled: replyTextField.text !== ""
                                    onClicked: {
                                        listitem.model.dbusInterface.sendReply(replyTextField.text);
                                        replyTextField.text = "";
                                        listitem.replying = false;
                                    }
                                }
                            }
                        }

                        CachyDivider {
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width - deviceHeaderContainer.leftPadding - deviceHeaderContainer.rightPadding
                            opacity: 0.5
                            visible: listitem.index < notificationsModel.count - 1
                        }
                    }
                }
            }
        }

        // Commands
        RowLayout {
            visible: remoteCommands.available && commandsModel.count > 0
            Layout.fillWidth: true
            spacing: CachyTheme.spacingSmall

            Text {
                text: i18n("Run command")
                font.pixelSize: 12
                font.weight: Font.Medium
                color: CachyTheme.textPrimary
                Layout.fillWidth: true
            }

            CachyToolbarButton {
                id: addCommandButton
                iconName: "list-add"
                tooltipText: i18n("Add command")
                onClicked: remoteCommands.plugin.editCommands()
                visible: remoteCommands.plugin?.canAddCommand ?? false
            }
        }

        Repeater {
            id: commandsView
            visible: remoteCommands.available

            model: KDEConnect.RemoteCommandsModel {
                id: commandsModel
                deviceId: root.model.deviceId
            }

            delegate: QQC2.ItemDelegate {
                id: commandDelegate

                required property int index
                required property var model

                enabled: true
                Layout.fillWidth: true

                background: Rectangle {
                    radius: CachyTheme.radiusSmall
                    color: commandDelegate.hovered ? CachyTheme.cardBackgroundHover : "transparent"
                }

                onClicked: {
                    remoteCommands.plugin?.triggerCommand(commandDelegate.model.key);
                }

                contentItem: Text {
                    text: `${commandDelegate.model.name}\n${commandDelegate.model.command}`
                    font.pixelSize: 12
                    color: CachyTheme.textPrimary
                }
            }
        }
    }
}
