/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Plasma Deck plugin configuration page, migrated to the CachyOS.UI design
 * system (Golden Gate / Liquid Glass).
 */

import QtQuick
import QtQuick.Layouts
import org.kde.kdeconnect
import CachyOS.UI

CachyFormCardPage {
    id: root

    property string device

    KdeConnectPluginConfig {
        id: config
        deviceId: root.device
        pluginName: "kdeconnect_plasmadeck"
    }

    CachyFormCard {
        Layout.fillWidth: true

        Text {
            text: i18nd("kdeconnect-plugins", "Plasma Deck converts your mobile phone into a touch companion and hardware telemetry display.")
            wrapMode: Text.WordWrap
            font.pixelSize: 12
            color: CachyTheme.textSecondary
            Layout.fillWidth: true
            Layout.minimumWidth: 0
        }

        CachyButton {
            Layout.fillWidth: true
            primary: true
            text: i18nd("kdeconnect-plugins", "Enviar y Abrir Plasma Deck en el móvil")
            iconName: "input-gaming"
            onClicked: {
                var service = "org.kde.kdeconnect";
                var path = "/modules/kdeconnect/devices/" + root.device + "/plasmadeck";
                var iface = "org.kde.kdeconnect.device.plasmadeck";
                qdbus.call(service, path, iface, "openPlasmaDeckOnPhone");
            }
        }
    }
}
