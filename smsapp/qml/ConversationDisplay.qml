/**
 * SPDX-FileCopyrightText: 2018 Aleix Pol Gonzalez <aleixpol@kde.org>
 * SPDX-FileCopyrightText: 2018 Nicolas Fella <nicolas.fella@gmx.de>
 * SPDX-FileCopyrightText: 2018 Simon Redman <simon@ergotech.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *
 * Conversation display page of the KDE Connect SMS application, migrated to
 * the CachyOS.UI design system (Golden Gate / Liquid Glass).
 */

import QtQuick
import QtQuick.Layouts
import org.kde.kdeconnect.sms
import CachyOS.UI

CachyScrollablePage {
    id: page

    title: SmsHelper.getTitleForAddresses(addresses)

    property bool deviceConnected: false
    property string conversationId
    property bool isMultitarget: false
    property string initialMessage
    property string invalidId: "-1"
    property var addresses

    ListView {
        id: viewport
        verticalLayoutDirection: ListView.BottomToTop
        boundsBehavior: Flickable.StopAtBounds
        reuseItems: true
        spacing: CachyTheme.spacingMedium

        model: ConversationModel {
            id: conversationModel
            deviceId: AppData.deviceId
            threadId: page.conversationId
            addressList: page.addresses
            onGotNewMessage: {
                let normalizedDistance = (1 - viewport.visibleArea.yPosition) / viewport.visibleArea.heightRatio - 1;
                if (normalizedDistance < 0.25) {
                    viewport.positionViewAtBeginning();
                }
            }
        }

        Timer {
            interval: 5000
            running: true
            repeat: true
            onTriggered: conversationModel.requestMessages(ConversationModel.UiTimer)
        }

        header: Item {
            height: CachyTheme.spacingLarge
        }
        headerPositioning: ListView.InlineHeader

        delegate: ChatMessage {
            required property var model
            required property int index

            name: model.sender
            messageBody: model.display
            sentByMe: model.fromMe
            dateTime: new Date(model.date)
            multiTarget: page.isMultitarget
            attachmentList: model.attachments
            conversationModel: conversationModel
            pageStack: page.pageStack

            width: viewport.width

            onMessageCopyRequested: (message) => {
                SmsHelper.copyToClipboard(message)
            }

            function prefetch() {
                const prefetchRange = 20;
                if (conversationModel.rowCount() - index + 1 < prefetchRange) {
                    conversationModel.requestMessages(ConversationModel.UiPrefetch);
                }
            }

            ListView.onAdd: prefetch()
            ListView.onReused: prefetch()
        }
    }

    footer: SendingArea {
        id: sendingArea
        addresses: page.addresses
        deviceConnected: page.deviceConnected
        conversationId: page.conversationId
        invalidId: page.invalidId
        conversationModel: conversationModel
    }
}
