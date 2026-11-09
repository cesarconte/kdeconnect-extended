/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "../plugins/panicshield/panicshieldplugin.h"
#include <QTest>
#include <core/networkpacket.h>

class PanicShieldTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testPanicPacketCreation()
    {
        NetworkPacket np(PACKET_TYPE_PANIC_SHIELD);
        np.set(QStringLiteral("action"), QStringLiteral("trigger"));

        QCOMPARE(np.type(), PACKET_TYPE_PANIC_SHIELD);
        QVERIFY(np.has(QStringLiteral("action")));
        QCOMPARE(np.get<QString>(QStringLiteral("action")), QStringLiteral("trigger"));
    }

    void testPanicActions()
    {
        const QStringList actions = {QStringLiteral("trigger"),
                                     QStringLiteral("restore"),
                                     QStringLiteral("toggle"),
                                     QStringLiteral("lock"),
                                     QStringLiteral("requestState")};

        for (const QString &act : actions) {
            NetworkPacket np(PACKET_TYPE_PANIC_SHIELD, {{QStringLiteral("action"), act}});
            QCOMPARE(np.get<QString>(QStringLiteral("action")), act);
        }
    }

    void testPanicStateSerialization()
    {
        NetworkPacket activeState(PACKET_TYPE_PANIC_SHIELD, {{QStringLiteral("isActive"), true}});
        QCOMPARE(activeState.get<bool>(QStringLiteral("isActive")), true);

        NetworkPacket idleState(PACKET_TYPE_PANIC_SHIELD, {{QStringLiteral("isActive"), false}});
        QCOMPARE(idleState.get<bool>(QStringLiteral("isActive")), false);
    }
};

QTEST_MAIN(PanicShieldTest)

#include "panicshieldtest.moc"
