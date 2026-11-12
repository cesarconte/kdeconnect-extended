/**
 * SPDX-FileCopyrightText: 2020 Aniket Kumar <anikketkumar786@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Message attachment item of the KDE Connect SMS application, migrated to the
 * CachyOS.UI design system (Golden Gate / Liquid Glass).
 */

import QtQuick
import QtQuick.Layouts
import QtMultimedia
import CachyOS.UI

Item {
    id: root
    property int partID
    property string mimeType
    property string uniqueIdentifier
    property url source
    property var conversationModel: null
    property var pageStack: null

    readonly property int elementWidth: 100
    readonly property int elementHeight: 100

    width: thumbnailElement.width
    height: thumbnailElement.height

    Component {
        id: attachmentViewer

        AttachmentViewer {
            fileUrl: root.source
            mimeType: root.mimeType
            title: root.uniqueIdentifier
            pageStack: root.pageStack
        }
    }

    Rectangle {
        id: thumbnailElement
        visible: root.mimeType.match("image") || root.mimeType.match("video")
        radius: 6
        color: CachyTheme.cardBackground
        clip: true
        width: visible && thumbImg.sourceSize.width > 0 ? thumbImg.sourceSize.width : root.elementWidth
        height: visible && thumbImg.sourceSize.height > 0 ? thumbImg.sourceSize.height : root.elementHeight

        Image {
            id: thumbImg
            anchors.fill: parent
            source: parent.visible ? "image://thumbnailsProvider/" + root.uniqueIdentifier : ""
            fillMode: Image.PreserveAspectFit
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (root.source.toString() === "") {
                    if (root.conversationModel) {
                        root.conversationModel.requestAttachmentPath(root.partID, root.uniqueIdentifier)
                    }
                } else {
                    root.openMedia();
                }
            }
        }

        CachyToolbarButton {
            iconName: "media-playback-start"
            visible: Boolean(root.mimeType.match("video"))
            anchors.centerIn: parent
            onClicked: {
                if (root.source.toString() === "") {
                    if (root.conversationModel) {
                        root.conversationModel.requestAttachmentPath(root.partID, root.uniqueIdentifier)
                    }
                } else {
                    root.openMedia();
                }
            }
        }
    }

    Rectangle {
        id: audioElement
        visible: Boolean(root.mimeType.match("audio"))
        anchors.fill: parent
        radius: CachyTheme.radiusSmall
        color: CachyTheme.cardBackground
        border.width: 1
        border.color: CachyTheme.specularBorder

        MediaPlayer {
            id: audioPlayer
            source: root.source

            onPlaybackStateChanged: {
                if (playbackState === MediaPlayer.PlayingState) {
                    audioPlayButton.iconName = "media-playback-stop"
                } else {
                    audioPlayButton.iconName = "media-playback-start"
                }
            }
        }

        ColumnLayout {
            anchors.centerIn: parent
            spacing: CachyTheme.spacingSmall

            CachyToolbarButton {
                id: audioPlayButton
                iconName: "media-playback-start"
                Layout.alignment: Qt.AlignCenter

                onClicked: {
                    if (root.source.toString() !== "") {
                        if (iconName === "media-playback-start") {
                            audioPlayer.play()
                        } else {
                            audioPlayer.stop()
                        }
                    } else if (root.conversationModel) {
                        root.conversationModel.requestAttachmentPath(root.partID, root.uniqueIdentifier)
                    }
                }
            }

            Text {
                text: i18nd("kdeconnect-sms", "Audio clip")
                font.pixelSize: 11
                color: CachyTheme.textSecondary
            }
        }
    }

    Connections {
        target: root.conversationModel
        function onFilePathReceived(filePath, fileName) {
            if (root.uniqueIdentifier === fileName && root.source.toString() === "") {
                root.source = "file://" + filePath

                if (root.mimeType.match("audio")) {
                    audioPlayer.source = root.source
                    audioPlayer.play()
                } else if (root.mimeType.match("image") || root.mimeType.match("video")) {
                    root.openMedia();
                }
            }
        }
    }

    function openMedia() {
        if (root.pageStack) {
            root.pageStack.layers.push(attachmentViewer, {})
        }
    }
}
