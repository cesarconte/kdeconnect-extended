/**
 * SPDX-FileCopyrightText: 2018 Aleix Pol Gonzalez <aleixpol@kde.org>
 * SPDX-FileCopyrightText: 2018 Nicolas Fella <nicolas.fella@gmx.de>
 * SPDX-FileCopyrightText: 2018 Simon Redman <simon@ergotech.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Conversation list page of the KDE Connect SMS application, migrated to the
 * CachyOS.UI design system (Golden Gate / Liquid Glass).
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kdeconnect.sms
import CachyOS.UI

CachyScrollablePage {
    id: page

    property string initialMessage: AppData.initialMessage
    property string currentSearchText
    property alias conversationListModel: conversationListModel
    property int devicesCount: 0
    readonly property bool deviceConnected: devicesCount > 0

    header: Item {
        implicitHeight: headerColumn.implicitHeight

        ColumnLayout {
            id: headerColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: CachyTheme.spacingSmall

            RowLayout {
                Layout.fillWidth: true
                visible: page.initialMessage.length > 0
                spacing: CachyTheme.spacingSmall

                CachyInlineMessage {
                    Layout.fillWidth: true
                    text: i18nd("kdeconnect-sms", "Choose recipient")
                }

                CachyButton {
                    text: i18nd("kdeconnect-sms", "Cancel")
                    onClicked: page.initialMessage = ""
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: CachyTheme.spacingSmall

                Keys.forwardTo: [filter]

                CachySearchField {
                    id: filter
                    Layout.fillWidth: true
                    placeholderText: i18nd("kdeconnect-sms", "Search or start a conversation")
                    enabled: page.deviceConnected
                    onTextChanged: {
                        page.currentSearchText = filter.text;
                        if (filter.text !== "") {
                            view.model.setConversationsFilterRole(ConversationListModel.AddressesRole)
                        } else {
                            view.model.setConversationsFilterRole(ConversationListModel.ConversationIdRole)
                        }
                        view.model.setFilterFixedString(SmsHelper.canonicalizePhoneNumber(filter.text))

                        view.currentIndex = 0
                        filter.forceActiveFocus();
                    }

                    Keys.onReturnPressed: event => {
                        event.accepted = true
                        if (view.currentItem) {
                            view.currentItem.startChat()
                        }
                    }
                    Keys.onEscapePressed: event => {
                        event.accepted = filter.text !== ""
                        filter.text = ""
                    }
                    Shortcut {
                        sequence: "Ctrl+F"
                        onActivated: filter.forceActiveFocus()
                    }
                }

                CachyToolbarButton {
                    id: newButton
                    iconName: "list-add-symbolic"
                    enabled: SmsHelper.isAddressValid(filter.text) && page.deviceConnected
                    tooltipText: i18nd("kdeconnect-sms", "Start new conversation")

                    onClicked: {
                        filter.enabled = false

                        if (!view.model.doesAddressExists(filter.text) && SmsHelper.isAddressValid(filter.text)) {
                            conversationListModel.createConversationForAddress(filter.text)
                            view.currentIndex = 0
                        }
                        if (view.currentItem) {
                            view.currentItem.startChat()
                        }
                        filter.enabled = true
                    }

                    Shortcut {
                        sequence: "Ctrl+N"
                        onActivated: newButton.clicked()
                    }
                }
            }
        }
    }

    Component {
        id: chatView
        ConversationDisplay {
            deviceConnected: page.deviceConnected
            pageStack: page.pageStack
        }
    }

    ListView {
        id: view
        model: QSortFilterProxyModel {
            sortOrder: Qt.DescendingOrder
            filterCaseSensitivity: Qt.CaseInsensitive
            sourceModel: ConversationListModel {
                id: conversationListModel
                deviceId: AppData.deviceId
            }
        }

        Component.onCompleted: {
            currentIndex = -1
            focus = true
        }

        delegate: QQC2.ItemDelegate {
            id: listItem
            text: displayNames
            width: view.width
            height: 56

            required property string displayNames
            required property string toolTip
            required property var decoration
            required property var attachmentPreview
            required property int index
            required property var addresses
            required property bool isMultitarget
            required property int conversationId

            highlighted: ListView.isCurrentItem

            function startChat() {
                if (view.currentItem) {
                    view.currentItem.forceActiveFocus();
                }
                if (page.pageStack) {
                    page.pageStack.push(chatView, {
                        addresses: listItem.addresses,
                        conversationId: listItem.conversationId,
                        isMultitarget: listItem.isMultitarget,
                        initialMessage: page.initialMessage,
                        pageStack: page.pageStack
                    })
                }
                page.initialMessage = ""
            }

            onClicked: {
                view.currentIndex = index
                startChat();
            }

            background: Rectangle {
                radius: CachyTheme.radiusSmall
                color: listItem.highlighted
                    ? CachyTheme.withAlpha(CachyTheme.accentColor, 0.16)
                    : (listItem.hovered ? CachyTheme.cardBackgroundHover : "transparent")
            }

            contentItem: RowLayout {
                id: contentRow
                spacing: CachyTheme.spacingSmall

                Kirigami.Icon {
                    id: contactIcon
                    source: listItem.decoration
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 0

                    Text {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: listItem.text
                        elide: Text.ElideRight
                        font.pixelSize: 13
                        font.weight: listItem.highlighted ? Font.Bold : Font.DemiBold
                        color: listItem.highlighted ? CachyTheme.accentColor : CachyTheme.textPrimary
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        visible: listItem.toolTip.length > 0
                        text: listItem.toolTip
                        elide: Text.ElideRight
                        font.pixelSize: 11
                        color: CachyTheme.textMuted
                    }
                }

                Rectangle {
                    id: thumbnailFrame
                    visible: listItem.attachmentPreview !== undefined
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    radius: 6
                    color: CachyTheme.cardBackground
                    clip: true

                    Image {
                        anchors.fill: parent
                        source: listItem.attachmentPreview === undefined ? "" : `image://thumbnailsProvider/${listItem.attachmentPreview}`
                        fillMode: Image.PreserveAspectCrop
                    }
                }
            }
        }
    }

    // =========================================================================
    // Estados vacíos / cargando
    // =========================================================================
    CachyPlaceholderMessage {
        parent: page
        anchors.centerIn: parent
        width: page.width - (CachyTheme.marginWindow * 2)
        visible: page.deviceConnected && view.count === 0 && page.currentSearchText.length !== 0
        iconName: "search"
        text: i18ndc("kdeconnect-sms", "Placeholder message text when no messages are found", "No Matches")
    }

    CachyPlaceholderMessage {
        parent: page
        anchors.centerIn: parent
        width: page.width - (CachyTheme.marginWindow * 2)
        visible: !page.deviceConnected && view.count === 0 && page.currentSearchText.length === 0
        iconName: "view-conversation-balloon"
        text: i18ndc("kdeconnect-sms", "@info:placeholder", "No Conversations")
    }

    CachyPlaceholderMessage {
        parent: page
        anchors.centerIn: parent
        width: page.width - (CachyTheme.marginWindow * 2)
        visible: page.deviceConnected && view.count === 0 && page.currentSearchText.length === 0
        iconName: "view-conversation-balloon"
        text: i18nd("kdeconnect-sms", "Loading conversations from device. If this takes a long time, please wake up your device and then click Refresh.")
        helpfulAction: ({
            text: i18nd("kdeconnect-sms", "Refresh"),
            onTriggered: () => conversationListModel.refresh()
        })

        QQC2.BusyIndicator {
            running: parent.visible
        }
    }
}
