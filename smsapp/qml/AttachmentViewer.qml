/**
 * SPDX-FileCopyrightText: 2020 Aniket Kumar <anikketkumar786@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Attachment viewer page of the KDE Connect SMS application, migrated to the
 * CachyOS.UI design system (Golden Gate / Liquid Glass).
 */

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kdeconnect.sms
import QtMultimedia
import CachyOS.UI

Item {
    id: root

    property url fileUrl
    property string mimeType
    property string title
    property var pageStack: null

    readonly property list<Kirigami.Action> headerActions: [
        Kirigami.Action {
            text: i18nd("kdeconnect-sms", "Open with default")
            icon.name: "window-new"
            onTriggered: {
                Qt.openUrlExternally(root.fileUrl);
            }
        },
        Kirigami.Action {
            text: i18nd("kdeconnect-sms", "Download attachment…")
            icon.name: "download"
            shortcut: StandardKey.Save
            onTriggered: {
                AttachmentsHelper.saveAttachment(root.mimeType, root.fileUrl);
            }
        }
    ]

    Rectangle {
        anchors.fill: parent
        radius: CachyTheme.radiusMedium
        color: CachyTheme.sheetBackground
        border.width: 1
        border.color: CachyTheme.specularBorder

        Image {
            id: image
            visible: Boolean(root.mimeType.match("image"))
            source: visible ? root.fileUrl : ""
            anchors.fill: parent
            anchors.margins: CachyTheme.marginCard
            fillMode: Image.PreserveAspectFit
        }

        MediaPlayer {
            id: mediaPlayer
            source: root.fileUrl

            onPositionChanged: {
                if (mediaPlayer.position > 1000 && mediaPlayer.duration - mediaPlayer.position < 1000) {
                    playAndPauseButton.iconName = "media-playback-start"
                    mediaPlayer.pause()
                    mediaPlayer.seek(0)
                }
            }
        }

        Item {
            visible: Boolean(root.mimeType.match("video"))
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: mediaControls.top

            MediaPlayer {
                source: mediaPlayer
                videoOutput: VideoOutput {
                    anchors.fill: parent
                    fillMode: VideoOutput.PreserveAspectFit
                    orientation: -90
                }
            }
        }

        Rectangle {
            id: mediaControls
            visible: Boolean(root.mimeType.match("video"))
            width: parent.width
            height: 60
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            color: CachyTheme.cardBackground

            CachyDivider {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: CachyTheme.spacingSmall
                spacing: CachyTheme.spacingSmall

                Rectangle {
                    id: progressBar
                    Layout.fillWidth: true
                    radius: 3
                    height: 6
                    color: CachyTheme.withAlpha(CachyTheme.textPrimary, 0.14)

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        radius: 3
                        width: mediaPlayer.duration > 0 ? parent.width * mediaPlayer.position / mediaPlayer.duration : 0
                        color: CachyTheme.accentColor
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: (mouse) => {
                            if (mediaPlayer.seekable) {
                                mediaPlayer.seek(mediaPlayer.duration * mouse.x / width);
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: CachyTheme.spacingMedium

                    CachyToolbarButton {
                        id: backwardButton
                        iconName: "media-seek-backward"
                        onClicked: {
                            if (mediaPlayer.seekable) {
                                mediaPlayer.seek(mediaPlayer.position - 2000)
                            }
                        }
                    }

                    CachyToolbarButton {
                        id: playAndPauseButton
                        iconName: "media-playback-pause"
                        primary: true
                        onClicked: {
                            if (iconName == "media-playback-start") {
                                mediaPlayer.play()
                                iconName = "media-playback-pause"
                            } else {
                                mediaPlayer.pause()
                                iconName = "media-playback-start"
                            }
                        }
                    }

                    CachyToolbarButton {
                        id: forwardButton
                        iconName: "media-seek-forward"
                        onClicked: {
                            if (mediaPlayer.seekable) {
                                mediaPlayer.seek(mediaPlayer.position + 2000)
                            }
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        mediaPlayer.play()
    }
}
