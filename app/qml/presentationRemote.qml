/*
 * SPDX-FileCopyrightText: 2018 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Presentation remote page of the KDE Connect application, migrated to the
 * CachyOS.UI design system (Golden Gate / Liquid Glass).
 */

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import CachyOS.UI

Item {
    id: mousepad

    property string title: i18nd("kdeconnect-app", "Presentation Remote")
    property QtObject pluginInterface
    property QtObject device

    readonly property list<Kirigami.Action> headerActions: [
        Kirigami.Action {
            icon.name: "view-fullscreen"
            text: i18nd("kdeconnect-app", "Enable Full-Screen")
            onTriggered: {
                mousepad.pluginInterface.sendKeyPress("", 25 /*XK_F5*/);
            }
        }
    ]

    ColumnLayout {
        anchors.fill: parent

        RowLayout {
            Layout.fillWidth: true
            spacing: CachyTheme.spacingSmall

            CachyButton {
                focus: true
                Layout.fillWidth: true
                iconName: "media-skip-backward"
                Accessible.name: i18nc("@action:button accessible", "Previous Slide")
                onClicked: mousepad.pluginInterface.sendKeyPress("p");
                KeyNavigation.right: forwardButton
            }
            CachyButton {
                id: forwardButton
                Layout.fillWidth: true
                primary: true
                iconName: "media-skip-forward"
                Accessible.name: i18nc("@action:button accessible", "Next Slide")
                onClicked: mousepad.pluginInterface.sendKeyPress("n");
            }
        }
    }
}
