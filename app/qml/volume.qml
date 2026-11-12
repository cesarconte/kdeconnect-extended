/*
 * SPDX-FileCopyrightText: 2018 Nicolas Fella <nicolas.fella@gmx.de>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Remote volume control page of the KDE Connect application, migrated to the
 * CachyOS.UI design system (Golden Gate / Liquid Glass).
 */

import QtQuick
import QtQuick.Layouts
import org.kde.kdeconnect
import CachyOS.UI

Item {
    id: root

    property string title: i18nd("kdeconnect-app", "Volume control")
    property QtObject pluginInterface
    property QtObject device

    ListView {
        id: sinkList
        anchors.fill: parent
        spacing: CachyTheme.spacingMedium
        focus: true

        model: RemoteSinksModel {
            deviceId: root.pluginInterface.deviceId
        }

        delegate: CachyCard {
            id: sinkCard
            required property string description
            required property bool muted
            required property int volume
            required property int maxVolume

            width: sinkList.width
            implicitHeight: cardColumn.implicitHeight + (CachyTheme.marginCard * 2)
            onActiveFocusChanged: if (activeFocus) muteButton.forceActiveFocus(Qt.TabFocusReason)

            ColumnLayout {
                id: cardColumn
                anchors.fill: parent
                spacing: CachyTheme.spacingSmall

                Text {
                    text: sinkCard.description
                    elide: Text.ElideRight
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: CachyTheme.textPrimary
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: CachyTheme.spacingSmall

                    CachyButton {
                        id: muteButton
                        iconName: sinkCard.muted ? "audio-volume-muted" : "audio-volume-high"
                        KeyNavigation.right: volumeSlider
                        onClicked: sinkCard.muted = !sinkCard.muted
                    }

                    CachySlider {
                        id: volumeSlider
                        Layout.fillWidth: true
                        from: 0
                        value: sinkCard.volume
                        to: sinkCard.maxVolume
                        onMoved: sinkCard.volume = value
                    }
                }
            }
        }
    }

    CachyPlaceholderMessage {
        anchors.centerIn: parent
        width: parent.width - (CachyTheme.marginWindow * 2)
        visible: sinkList.count === 0
        iconName: "audio-volume-high"
        text: i18nd("kdeconnect-app", "Volume control")
    }
}
