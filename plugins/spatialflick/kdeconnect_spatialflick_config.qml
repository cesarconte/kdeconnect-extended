/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Spatial Flick plugin configuration page, migrated to the CachyOS.UI design
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
        pluginName: "kdeconnect_spatialflick"
    }

    Component.onCompleted: {
        portalEnabledCheck.checked = config.getBool("portalEnabled", true)
        soundEnabledCheck.checked = config.getBool("soundEnabled", true)
        const edge = config.getString("edgePosition", "right")
        if (edge === "left") edgeCombo.currentIndex = 1
        else if (edge === "top") edgeCombo.currentIndex = 2
        else if (edge === "bottom") edgeCombo.currentIndex = 3
        else edgeCombo.currentIndex = 0
    }

    CachyFormHeader {
        Layout.fillWidth: true
        title: i18nd("kdeconnect-plugins", "Spatial Flick & Drop (Transferencia Direccional):")
    }

    CachyFormCard {
        Layout.fillWidth: true

        CachyFormCheckDelegate {
            id: portalEnabledCheck
            text: i18nd("kdeconnect-plugins", "Habilitar portal de borde de pantalla para arrastre de archivos")
            onToggled: (next) => {
                checked = next
                config.set("portalEnabled", next)
            }
        }

        CachyFormDelegateSeparator {}

        RowLayout {
            Layout.fillWidth: true
            spacing: CachyTheme.spacingMedium

            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: i18nd("kdeconnect-plugins", "Borde de la pantalla donde se ubica el móvil:")
                wrapMode: Text.WordWrap
                font.pixelSize: 12
                color: CachyTheme.textPrimary
            }

            QQC2.ComboBox {
                id: edgeCombo
                model: [
                    i18nd("kdeconnect-plugins", "Borde Derecho (predeterminado)"),
                    i18nd("kdeconnect-plugins", "Borde Izquierdo"),
                    i18nd("kdeconnect-plugins", "Borde Superior"),
                    i18nd("kdeconnect-plugins", "Borde Inferior")
                ]
                onActivated: (index) => {
                    if (index === 1) config.set("edgePosition", "left")
                    else if (index === 2) config.set("edgePosition", "top")
                    else if (index === 3) config.set("edgePosition", "bottom")
                    else config.set("edgePosition", "right")
                }
            }
        }

        CachyFormDelegateSeparator {}

        CachyFormCheckDelegate {
            id: soundEnabledCheck
            text: i18nd("kdeconnect-plugins", "Efectos de sonido al transferir archivos")
            onToggled: (next) => {
                checked = next
                config.set("soundEnabled", next)
            }
        }
    }
}
