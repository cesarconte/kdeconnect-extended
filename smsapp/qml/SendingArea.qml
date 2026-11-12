/**
 * SPDX-FileCopyrightText: 2020 Aniket Kumar <anikketkumar786@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Message composition area of the KDE Connect SMS application, migrated to
 * the CachyOS.UI design system (Golden Gate / Liquid Glass).
 */

import QtQuick
import QtCore
import QtQuick.Controls as Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import org.kde.kdeconnect.sms
import CachyOS.UI

Item {
    id: root

    property var addresses
    property var selectedFileUrls: []
    readonly property int maxMessageSize: 600000
    property alias text: messageField.text

    property bool deviceConnected: false
    property string conversationId: "-1"
    property string invalidId: "-1"
    property var conversationModel: null

    implicitHeight: sendingRow.implicitHeight + 16

    MessageDialog {
        id: messageDialog
        title: i18nd("kdeconnect-sms", "Failed to send")
        text: i18nd("kdeconnect-sms", "Max message size limit exceeded.")
        buttons: MessageDialog.Ok

        onAccepted: {
            messageDialog.close()
        }
    }

    FileDialog {
        id: fileDialog
        currentFolder: StandardPaths.standardLocations(StandardPaths.HomeLocation)[0]
        fileMode: FileDialog.OpenFiles

        onAccepted: {
            root.selectedFileUrls = fileDialog.selectedFiles
            fileDialog.close()
        }
    }

    RowLayout {
        id: sendingRow
        anchors.fill: parent
        anchors.topMargin: 8
        anchors.bottomMargin: 8
        enabled: root.deviceConnected
        spacing: CachyTheme.spacingSmall

        CachyTextField {
            id: messageField
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            placeholderText: i18nd("kdeconnect-sms", "Send a message…")

            Keys.onReturnPressed: event => {
                if (event.key === Qt.Key_Return) {
                    if (!(event.modifiers & Qt.ShiftModifier)) {
                        sendButton.clicked()
                        event.accepted = true
                    }
                }
            }
        }

        CachyToolbarButton {
            id: attachFilesButton
            iconName: "mail-attachment-symbolic"
            text: root.selectedFileUrls.length > 0 ? String(root.selectedFileUrls.length) : ""
            tooltipText: i18nc("@info:tooltip", "Attach an image or file")
            onClicked: fileDialog.open()
        }

        CachyToolbarButton {
            id: clearAttachmentButton
            visible: root.selectedFileUrls.length > 0
            iconName: "edit-clear"
            tooltipText: i18nc("@info:tooltip", "Clear attachments")
            onClicked: {
                root.selectedFileUrls = []
            }
        }

        CachyToolbarButton {
            id: sendButton
            property bool isSendingInProcess: false
            property bool messageSent: false

            primary: true
            enabled: (messageField.text.length > 0 || root.selectedFileUrls.length > 0) && !isSendingInProcess
            iconName: "document-send"
            tooltipText: i18nc("@info:tooltip", "Send message")

            onClicked: {
                if (isSendingInProcess || !root.conversationModel) {
                    return;
                }
                isSendingInProcess = true

                if (SmsHelper.totalMessageSize(root.selectedFileUrls, messageField.text) > root.maxMessageSize) {
                    messageDialog.open()
                } else if (root.conversationId === root.invalidId) {
                    messageSent = root.conversationModel.startNewConversation(messageField.text, root.addresses, root.selectedFileUrls)
                } else {
                    messageSent = root.conversationModel.sendReplyToConversation(messageField.text, root.selectedFileUrls)
                }

                if (messageSent) {
                    messageField.text = ""
                    root.selectedFileUrls = []
                }
                isSendingInProcess = false
            }
        }

        Text {
            id: charCount
            text: root.conversationModel ? root.conversationModel.getCharCountInfo(messageField.text) : ""
            visible: text.length > 0
            font.pixelSize: 11
            color: CachyTheme.textMuted
        }
    }
}
