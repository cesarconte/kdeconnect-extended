/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami
import org.kde.kdeconnect 1.0

Kirigami.ScrollablePage {
    id: root

    property string device

    Component.onCompleted: {
        portalEnabledCheck.checked = config.getBool("portalEnabled", true)
        soundEnabledCheck.checked = config.getBool("soundEnabled", true)
        const edge = config.getString("edgePosition", "right")
        if (edge === "left") edgeCombo.currentIndex = 1
        else if (edge === "top") edgeCombo.currentIndex = 2
        else if (edge === "bottom") edgeCombo.currentIndex = 3
        else edgeCombo.currentIndex = 0
    }

    Kirigami.FormLayout {
        KdeConnectPluginConfig {
            id: config
            deviceId: device
            pluginName: "kdeconnect_spatialflick"
        }

        QQC2.Label {
            text: i18nd("kdeconnect-plugins", "Spatial Flick & Drop (Transferencia Direccional):")
            font.bold: true
        }

        QQC2.CheckBox {
            id: portalEnabledCheck
            text: i18nd("kdeconnect-plugins", "Habilitar portal de borde de pantalla para arrastre de archivos")
            onClicked: config.set("portalEnabled", checked)
        }

        QQC2.ComboBox {
            id: edgeCombo
            Kirigami.FormData.label: i18nd("kdeconnect-plugins", "Borde de la pantalla donde se ubica el móvil:")
            model: [
                i18nd("kdeconnect-plugins", "Borde Derecho (predeterminado)"),
                i18nd("kdeconnect-plugins", "Borde Izquierdo"),
                i18nd("kdeconnect-plugins", "Borde Superior"),
                i18nd("kdeconnect-plugins", "Borde Inferior")
            ]
            onActivated: {
                if (index === 1) config.set("edgePosition", "left")
                else if (index === 2) config.set("edgePosition", "top")
                else if (index === 3) config.set("edgePosition", "bottom")
                else config.set("edgePosition", "right")
            }
        }

        QQC2.CheckBox {
            id: soundEnabledCheck
            text: i18nd("kdeconnect-plugins", "Efectos de sonido al transferir archivos")
            onClicked: config.set("soundEnabled", checked)
        }
    }
}
