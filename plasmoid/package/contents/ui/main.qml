/**
 * SPDX-FileCopyrightText: 2014 Aleix Pol Gonzalez <aleixpol@kde.org>
 * SPDX-FileCopyrightText: 2024 ivan tkachenko <me@ratijas.tk>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Root item of the KDE Connect plasmoid, migrated to the CachyOS.UI design
 * system (Golden Gate / Liquid Glass).
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import org.kde.config as KConfig
import org.kde.kdeconnect as KDEConnect
import org.kde.kquickcontrolsaddons as KQuickControlsAddons
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid

PlasmoidItem {
    id: root

    readonly property bool inPanel: [
        PlasmaCore.Types.TopEdge,
        PlasmaCore.Types.RightEdge,
        PlasmaCore.Types.BottomEdge,
        PlasmaCore.Types.LeftEdge,
    ].includes(Plasmoid.location)

    // Permite mantener fijada la ventana abierta para arrastrar archivos sin auto-cierre
    property bool isPinned: true
    hideOnWindowDeactivate: !isPinned

    KDEConnect.DevicesModel {
        id: connectedDeviceModel
        displayFilter: KDEConnect.DevicesModel.Paired | KDEConnect.DevicesModel.Reachable
    }

    KDEConnect.DevicesModel {
        id: pairedDeviceModel
        displayFilter: KDEConnect.DevicesModel.Paired
    }

    Plasmoid.icon: inPanel
        ? "kdeconnect-tray-symbolic"
        : "kdeconnect-tray"

    Plasmoid.status: connectedDeviceModel.count > 0 ? PlasmaCore.Types.ActiveStatus : PlasmaCore.Types.PassiveStatus

    fullRepresentation: FullRepresentation {
        plasmoidItem: root
        Plasmoid.backgroundHints: root.inPanel ? PlasmaCore.Types.NoBackground : PlasmaCore.Types.DefaultBackground
        devicesModel: connectedDeviceModel
        Layout.minimumWidth: 342
        Layout.minimumHeight: 396
        Layout.preferredWidth: 396
        Layout.preferredHeight: 450
    }

    compactRepresentation: CompactRepresentation {
        plasmoidItem: root
    }

    PlasmaCore.Action {
        id: configureAction
        text: i18n("KDE Connect Settings…")
        icon.name: "configure"
        onTriggered: KDEConnect.OpenConfig.openConfiguration()
    }

    Component.onCompleted: {
        Plasmoid.setInternalAction("configure", configureAction);
    }
}
