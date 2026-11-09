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
    property string device

    Kirigami.FormLayout {

        KdeConnectPluginConfig {
            id: config
            deviceId: device
            pluginName: "kdeconnect_plasmadeck"
        }

        QQC2.Label {
            text: i18nd("kdeconnect-plugins", "Plasma Deck converts your mobile phone into an Elgato Stream Deck style touch companion and hardware telemetry display.")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        QQC2.Button {
            text: i18nd("kdeconnect-plugins", "🚀 Enviar y Abrir Plasma Deck en el móvil")
            icon.name: "input-gaming"
            onClicked: {
                var service = "org.kde.kdeconnect";
                var path = "/modules/kdeconnect/devices/" + device + "/plasmadeck";
                var iface = "org.kde.kdeconnect.device.plasmadeck";
                // Trigger via D-Bus call
                qdbus.call(service, path, iface, "openPlasmaDeckOnPhone");
            }
        }
    }
}
