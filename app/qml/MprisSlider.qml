/**
 * SPDX-FileCopyrightText: 2019 Nicolas Fella <nicolas.fella@gmx.de>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Multimedia progress and seek control, migrated to the CachyOS.UI design
 * system (Golden Gate / Liquid Glass).
 */

import QtQuick
import CachyOS.UI

Loader {
    id: root

    property var plugin
    property var lastPosition: plugin.position
    property date lastPositionTime: new Date()
    property bool updatePositionSlider: true

    sourceComponent: plugin.canSeek ? seekBar : progressBar

    onLastPositionChanged: {
        if (item != null) {
            item.value = lastPosition
            lastPositionTime = new Date();
        }
    }

    Component {
        id: seekBar

        CachySlider {
            from: 0
            to: root.plugin.length
            onPressedChanged: {
                if (pressed) {
                    root.updatePositionSlider = false
                } else {
                    root.updatePositionSlider = true
                    root.plugin.position = value
                }
            }
        }
    }

    Component {
        id: progressBar

        CachyProgressBar {
            from: 0
            to: root.plugin.length
        }
    }

    Timer {
        id: positionUpdateTimer
        interval: 1000
        repeat: true
        running: root.updatePositionSlider && root.plugin.isPlaying

        onTriggered: {
            if (root.item) {
                root.item.value = root.lastPosition + (new Date().getTime() - root.lastPositionTime.getTime())
            }
        }
    }
}
