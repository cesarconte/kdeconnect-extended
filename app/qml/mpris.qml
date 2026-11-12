/*
 * SPDX-FileCopyrightText: 2015 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Multimedia remote control page of the KDE Connect application, migrated to
 * the CachyOS.UI design system (Golden Gate / Liquid Glass).
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import CachyOS.UI

Item {
    id: root

    property QtObject pluginInterface
    property QtObject device
    property bool muted: false
    property int volumeUnmuted
    property var volume: pluginInterface.volume
    property string title: i18nd("kdeconnect-app", "Multimedia Controls")

    onVolumeChanged: {
        if (muted && volume != 0) {
            toggleMute()
            volumeUnmuted = volume
        } else if (!muted) {
            volumeUnmuted = volume
        }
    }

    function soundState(volume)
    {
        if (volume <= 25) {
            return "audio-volume-low"
        } else if (volume <= 75) {
            return "audio-volume-medium"
        } else {
            return "audio-volume-high"
        }
    }

    function toggleMute()
    {
        muted = !muted
        root.pluginInterface.volume = muted ? 0 : volumeUnmuted
    }

    function padTimeNumber(n) {
        return (n < 10) ? ("0" + n) : n;
    }

    function msToTime(totalTimeMs)
    {
        if (!totalTimeMs || totalTimeMs < 0) {
            return "00:00";
        }
        let hours = Math.floor(totalTimeMs/(1000*60*60));
        let minutes = Math.floor((totalTimeMs-(hours*1000*60*60))/(1000*60));
        let seconds = Math.floor((totalTimeMs-(minutes*1000*60)-(hours*1000*60*60))/1000);
        if (hours > 0) {
            return `${padTimeNumber(hours)}:${padTimeNumber(minutes)}:${padTimeNumber(seconds)}`;
        } else {
            return `${padTimeNumber(minutes)}:${padTimeNumber(seconds)}`;
        }
    }

    CachyPlaceholderMessage {
        id: noPlayersText
        text: i18nd("kdeconnect-app", "No players available")
        iconName: "multimedia-player-symbolic"
        anchors.centerIn: parent
        width: parent.width - (CachyTheme.marginWindow * 2)
        visible: pluginInterface.playerList.length == 0
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: CachyTheme.spacingMedium
        visible: !noPlayersText.visible

        Component.onCompleted: {
            pluginInterface.requestPlayerList();
        }

        Item { Layout.fillHeight: true }

        CachyCard {
            Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
            Layout.preferredHeight: 220
            Layout.preferredWidth: 220

            Image {
                id: albumArt
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
                source: root.pluginInterface.localAlbumArtUrl
                visible: !!root.pluginInterface.localAlbumArtUrl && root.pluginInterface.localAlbumArtUrl.length > 0
                smooth: true
            }

            Image {
                anchors.centerIn: parent
                width: 64
                height: 64
                source: CachyTheme.iconUrl("multimedia-player-symbolic")
                fillMode: Image.PreserveAspectFit
                visible: !albumArt.visible
                opacity: 0.45
            }
        }

        QQC2.ComboBox {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            model: root.pluginInterface.playerList
            onCurrentTextChanged: root.pluginInterface.player = currentText
            focus: true
            KeyNavigation.down: mediaBackButton
            KeyNavigation.priority: KeyNavigation.BeforeItem
        }

        CachyHeading {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: root.pluginInterface.title
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            level: 2
        }

        Text {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: root.pluginInterface.artist
            visible: !artistAlbum.visible && root.pluginInterface.artist.length > 0
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 12
            color: CachyTheme.textSecondary
        }

        Text {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: root.pluginInterface.album
            visible: !artistAlbum.visible && root.pluginInterface.album.length > 0
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 12
            color: CachyTheme.textSecondary
        }

        Text {
            id: artistAlbum
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: i18nd("kdeconnect-app", "%1 - %2", root.pluginInterface.artist, root.pluginInterface.album)
            visible: root.pluginInterface.album.length > 0 && root.pluginInterface.artist.length > 0
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 12
            color: CachyTheme.textSecondary
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: CachyTheme.spacingSmall
            // player controls are by convention always LtR
            LayoutMirroring.enabled: false

            CachyButton {
                id: mediaBackButton
                Layout.fillWidth: true
                iconName: "media-skip-backward"
                onClicked: root.pluginInterface.sendAction("Previous")
                KeyNavigation.right: mediaPlayButton
                KeyNavigation.down: positionIndicator
                Keys.onRightPressed: mediaPlayButton.forceActiveFocus(Qt.TabFocusReason)
            }
            CachyButton {
                id: mediaPlayButton
                Layout.fillWidth: true
                primary: true
                iconName: root.pluginInterface.isPlaying ? "media-playback-pause" : "media-playback-start"
                onClicked: root.pluginInterface.sendAction("PlayPause");
                Keys.onLeftPressed: mediaBackButton.forceActiveFocus(Qt.TabFocusReason)
                Keys.onRightPressed: mediaForwardButton.forceActiveFocus(Qt.TabFocusReason)
                KeyNavigation.down: positionIndicator
            }
            CachyButton {
                id: mediaForwardButton
                Layout.fillWidth: true
                iconName: "media-skip-forward"
                onClicked: root.pluginInterface.sendAction("Next")
                KeyNavigation.down: positionIndicator
                Keys.onLeftPressed: mediaPlayButton.forceActiveFocus(Qt.TabFocusReason)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: CachyTheme.spacingSmall
            // media progress indicators are by convention always LtR
            LayoutMirroring.enabled: false
            LayoutMirroring.childrenInherit: true

            Text {
                text: positionIndicator.item ? msToTime(positionIndicator.item.value) : "00:00"
                font.pixelSize: 11
                color: CachyTheme.textMuted
            }

            MprisSlider {
                id: positionIndicator
                plugin: root.pluginInterface
                Layout.fillWidth: true
                KeyNavigation.up: mediaBackButton
                KeyNavigation.down: muteButton
            }

            Text {
                text: msToTime(root.pluginInterface.length)
                font.pixelSize: 11
                color: CachyTheme.textMuted
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: CachyTheme.spacingSmall

            CachyButton {
                id: muteButton
                iconName: muted ? "audio-volume-muted" : soundState(root.pluginInterface.volume)
                onClicked: toggleMute()
                KeyNavigation.right: volumeSlider
            }
            CachySlider {
                id: volumeSlider
                value: volumeUnmuted
                to: 99
                enabled: !muted
                Layout.fillWidth: true
                onValueChanged: {
                    volumeUnmuted = value
                    root.pluginInterface.volume = value
                }
                KeyNavigation.up: positionIndicator
            }
        }

        Item { Layout.fillHeight: true }
    }

    Component.onCompleted: volumeUnmuted = root.pluginInterface.volume
}
