/**
 * SPDX-FileCopyrightText: 2020 Nicolas Fella <nicolas.fella@gmx.de>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Chat message bubble of the KDE Connect SMS application, migrated to the
 * CachyOS.UI design system (Golden Gate / Liquid Glass).
 */

import QtQuick
import QtQuick.Layouts
import CachyOS.UI

Item {
    id: root

    height: Math.max(avatar.height, messageBubble.height)

    property string messageBody
    property bool sentByMe
    property string selectedText
    property date dateTime
    property string name
    property bool multiTarget
    property var attachmentList: []
    property var conversationModel: null
    property var pageStack: null

    signal messageCopyRequested(string message)

    function linkify(text) {
        let s = text.replace(/[&<>]/g, function(c) {
            return {"&": "&amp;", "<": "&lt;", ">": "&gt;"}[c];
        });

        s = s.replace(/(https?:\/\/[^\s<>]*[^\s<>.,;:!?\)\]"']|www\.[^\s<>]*[^\s<>.,;:!?\)\]"'])/gi, function(url) {
            const href = url.startsWith("www.") ? "https://" + url : url;
            return '<a href="' + href + '">' + url + '</a>';
        });

        return s.replace(/\n/g, "<br>");
    }

    Rectangle {
        id: avatar
        width: visible ? 32 : 0
        height: width
        radius: width / 2
        visible: !root.sentByMe && root.multiTarget
        color: CachyTheme.withAlpha(CachyTheme.accentColor, 0.20)
        border.width: 1
        border.color: CachyTheme.specularBorder

        anchors.left: parent.left
        anchors.leftMargin: CachyTheme.spacingMedium

        Text {
            anchors.centerIn: parent
            text: root.name.length > 0 ? root.name.charAt(0).toUpperCase() : "?"
            font.pixelSize: 13
            font.weight: Font.DemiBold
            color: CachyTheme.textPrimary
        }
    }

    RowLayout {
        id: messageBubble

        anchors.right: parent.right
        anchors.rightMargin: CachyTheme.spacingMedium
        anchors.left: avatar.right
        anchors.leftMargin: CachyTheme.spacingMedium

        height: messageColumn.height

        Rectangle {
            id: messageBox
            Layout.maximumWidth: root.width > 540 ? Math.min(messageColumn.contentWidth, root.width * 0.6) : messageColumn.contentWidth
            Layout.fillWidth: true
            Layout.alignment: root.sentByMe ? Qt.AlignRight : Qt.AlignLeft
            Layout.fillHeight: true
            Layout.minimumWidth: dateLabel.implicitWidth

            color: root.sentByMe
                ? CachyTheme.withAlpha(CachyTheme.accentColor, 0.22)
                : CachyTheme.cardBackground
            radius: CachyTheme.radiusMedium
            border.width: 1
            border.color: CachyTheme.specularBorder

            Column {
                id: messageColumn
                width: parent.width

                property int contentWidth: Math.max(Math.max(messageLabel.implicitWidth, attachmentGrid.implicitWidth), dateLabel.implicitWidth)

                Text {
                    id: authorLabel
                    width: parent.width
                    text: root.name
                    leftPadding: CachyTheme.spacingMedium
                    topPadding: CachyTheme.spacingSmall
                    visible: root.multiTarget
                    font.pixelSize: 11
                    font.weight: Font.Medium
                    color: CachyTheme.textMuted
                    horizontalAlignment: messageLabel.horizontalAlignment
                }

                Grid {
                    id: attachmentGrid
                    columns: 2
                    padding: root.attachmentList.length > 0 ? CachyTheme.spacingMedium : 0
                    layoutDirection: root.sentByMe ? Qt.RightToLeft : Qt.LeftToRight

                    Repeater {
                        model: root.attachmentList

                        delegate: MessageAttachments {
                            required property var modelData
                            mimeType: modelData.mimeType
                            partID: modelData.partID
                            uniqueIdentifier: modelData.uniqueIdentifier
                            conversationModel: root.conversationModel
                            pageStack: root.pageStack
                        }
                    }
                }

                CachySelectableLabel {
                    id: messageLabel
                    visible: root.messageBody != ""
                    leftPadding: CachyTheme.spacingMedium
                    rightPadding: CachyTheme.spacingMedium
                    topPadding: authorLabel.visible ? 0 : CachyTheme.spacingMedium
                    width: parent.width
                    horizontalAlignment: root.sentByMe ? Text.AlignRight : Text.AlignLeft
                    wrapMode: Text.Wrap
                    textFormat: Text.RichText
                    text: root.linkify(root.messageBody)
                    onLinkActivated: (link) => Qt.openUrlExternally(link)
                }

                Text {
                    id: dateLabel
                    width: parent.width
                    text: Qt.formatDateTime(root.dateTime)
                    leftPadding: CachyTheme.spacingMedium
                    rightPadding: CachyTheme.spacingMedium
                    bottomPadding: CachyTheme.spacingSmall
                    font.pixelSize: 10
                    color: CachyTheme.textMuted
                    horizontalAlignment: messageLabel.horizontalAlignment
                }
            }
        }
    }
}
