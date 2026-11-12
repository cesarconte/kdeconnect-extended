/**
 * SPDX-FileCopyrightText: 2026 Aleix Pol i Gonzalez <aleixpol@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Presenter plugin configuration page, migrated to the CachyOS.UI design
 * system (Golden Gate / Liquid Glass).
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kdeconnect
import CachyOS.UI

CachyFormCardPage {
    id: root

    property string device

    KdeConnectPluginConfig {
        id: config
        deviceId: root.device
        pluginName: "kdeconnect_presenter"
    }

    CachyFormCard {
        Layout.fillWidth: true

        RowLayout {
            Layout.fillWidth: true
            spacing: CachyTheme.spacingMedium

            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: i18nc("@action:label", "Display:")
                font.pixelSize: 12
                color: CachyTheme.textPrimary
            }

            QQC2.ComboBox {
                readonly property var sources: [
                    { name: "PresenterRedDot.qml", display: i18nd("kdeconnect-plugins", "Red Dot") },
                    { name: "PresenterElectric.qml", display: i18nd("kdeconnect-plugins", "Electric") },
                ]
                model: sources
                textRole: "display"
                currentIndex: {
                    const currentName = config.getString("current", "")
                    return Math.max(sources.findIndex(item => item.name === currentName), 0)
                }
                onActivated: index => config.set("current", sources[index].name)
            }
        }
    }
}
