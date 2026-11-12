/*
 * SPDX-FileCopyrightText: 2019 Nicolas Fella <nicolas.fella@gmx.de>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Notifications page of the KDE Connect application, migrated to the
 * CachyOS.UI design system (Golden Gate / Liquid Glass).
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kdeconnect
import org.kde.kdeconnect as KDEConnect
import org.kde.kirigami as Kirigami
import CachyOS.UI

CachyScrollablePage {
    id: page

    required property var pluginInterface
    required property var device

    title: i18nc("@title:window", "Notifications")

    contentColumn.spacing: CachyTheme.spacingSmall

    CachyPlaceholderMessage {
        parent: page
        anchors.centerIn: parent
        width: parent.width - (CachyTheme.marginWindow * 2)
        visible: notificationsView.count === 0
        iconName: "notifications"
        text: i18nc("@info:placeholder", "No notifications")
    }

    Repeater {
        id: notificationsView

        model: KDEConnect.NotificationsModel {
            id: notificationsModel
            deviceId: page.device.id()
        }

        delegate: CachyCard {
            id: listitem

            required property int index
            required property var model
            property bool replying: false

            Layout.fillWidth: true
            implicitHeight: cardBody.implicitHeight + (CachyTheme.marginCard * 2)
            clip: true

            SequentialAnimation {
                id: dismissAnimation

                NumberAnimation {
                    target: listitem
                    property: "x"
                    to: listitem.LayoutMirroring.enabled ? -listitem.width : listitem.width
                    duration: CachyTheme.animNormal
                }

                ScriptAction {
                    script: listitem.model.dbusInterface.dismiss()
                }
            }

            Behavior on height {
                NumberAnimation {
                    duration: CachyTheme.animNormal
                    easing.type: Easing.InOutQuad
                }
            }

            ColumnLayout {
                id: cardBody
                anchors.fill: parent
                spacing: CachyTheme.spacingSmall

                RowLayout {
                    id: notificationHead
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: CachyTheme.spacingMedium

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: CachyTheme.spacingTiny

                        RowLayout {
                            Layout.fillWidth: true
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
                                elide: Text.ElideRight
                                maximumLineCount: 1
                                wrapMode: Text.Wrap
                                Layout.alignment: Qt.AlignVCenter
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                color: CachyTheme.textSecondary
                            }
                        }

                        CachySelectableLabel {
                            text: listitem.model.title
                            textFormat: Text.PlainText
                            visible: text.length > 0
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
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
                        enabled: listitem.model.dismissable
                        Layout.alignment: Qt.AlignTop
                        iconName: "window-close"
                        tooltipText: i18n("Dismiss")
                        onClicked: dismissAnimation.start()
                    }
                }

                CachySelectableLabel {
                    text: listitem.model.notitext
                    visible: text.length > 0
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
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
                        Keys.onPressed: (event) => {
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
                        id: replySendButton
                        Layout.alignment: Qt.AlignBottom
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
        }
    }
}
