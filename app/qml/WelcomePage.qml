/*
 * SPDX-FileCopyrightText: 2024 Darshan Phaldesai <dev.darshanphaldesai@gmail.org>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Welcome page of the KDE Connect application, migrated to the CachyOS.UI
 * design system (Golden Gate / Liquid Glass).
 */

import QtQuick
import QtQuick.Layouts
import CachyOS.UI

CachyFormCardPage {
    id: root

    Image {
        // The icon of the application is installed by its own package; when it
        // is not available a generic one is shown instead of a hole.
        source: CachyTheme.iconUrl("kdeconnect", "network-connect-symbolic")
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: 96
        Layout.preferredHeight: 96
        Layout.topMargin: CachyTheme.spacingLarge
        fillMode: Image.PreserveAspectFit
        smooth: true
    }

    CachyHeading {
        text: i18nc("@title", "No Device Selected")
        horizontalAlignment: Text.AlignHCenter
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.topMargin: CachyTheme.spacingLarge
        level: 2
    }

    Text {
        text: i18nc("@info", "Make sure to install KDE Connect on your other devices.")
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.topMargin: CachyTheme.spacingMedium
        font.pixelSize: 12
        color: CachyTheme.textSecondary
    }

    CachyFormHeader {
        title: i18nc("@title:context", "KDE Connect for Android")
        Layout.fillWidth: true
        Layout.leftMargin: CachyTheme.marginWindow
        Layout.rightMargin: CachyTheme.marginWindow
        Layout.topMargin: CachyTheme.spacingLarge
    }

    CachyFormCard {
        Layout.fillWidth: true
        Layout.leftMargin: CachyTheme.marginWindow
        Layout.rightMargin: CachyTheme.marginWindow

        CachyFormButtonDelegate {
            text: i18nc("Android Application Store", "F-Droid")
            onClicked: Qt.openUrlExternally("https://f-droid.org/repository/browse/?fdid=org.kde.kdeconnect_tp")
        }

        CachyFormDelegateSeparator {}

        CachyFormButtonDelegate {
            text: i18nc("Android Application Store", "Google Play")
            onClicked: Qt.openUrlExternally("https://play.google.com/store/apps/details?id=org.kde.kdeconnect_tp")
        }
    }
}
