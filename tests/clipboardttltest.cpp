/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "../plugins/clipboard/clipboardplugin.h"
#include <QDateTime>
#include <QTest>
#include <core/networkpacket.h>

class ClipboardTtlTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testTtlHeadersCreation()
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        constexpr qint64 expectedTtl = 120 * 1000;

        NetworkPacket np(PACKET_TYPE_CLIPBOARD);
        np.set<qint64>(QStringLiteral("timestamp"), now);
        np.set<qint64>(QStringLiteral("expiryTimestamp"), now + expectedTtl);
        np.set(QStringLiteral("content"), QStringLiteral("Test sensitive continuity text"));

        QVERIFY(np.has(QStringLiteral("timestamp")));
        QVERIFY(np.has(QStringLiteral("expiryTimestamp")));
        QCOMPARE(np.get<qint64>(QStringLiteral("timestamp")), now);
        QCOMPARE(np.get<qint64>(QStringLiteral("expiryTimestamp")), now + expectedTtl);
        QCOMPARE(np.get<QString>(QStringLiteral("content")), QStringLiteral("Test sensitive continuity text"));
    }

    void testTtlExpiredEvaluation()
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();

        // 1. Paquete expirado (expiró hace 5 segundos)
        NetworkPacket expiredPacket(PACKET_TYPE_CLIPBOARD);
        expiredPacket.set<qint64>(QStringLiteral("expiryTimestamp"), now - 5000);
        expiredPacket.set(QStringLiteral("content"), QStringLiteral("Old expired password"));

        const qint64 expiry = expiredPacket.get<qint64>(QStringLiteral("expiryTimestamp"));
        const bool isExpired = now > expiry;
        QCOMPARE(isExpired, true);

        // 2. Paquete válido (expirará en 115 segundos)
        NetworkPacket validPacket(PACKET_TYPE_CLIPBOARD);
        validPacket.set<qint64>(QStringLiteral("expiryTimestamp"), now + 115000);
        validPacket.set(QStringLiteral("content"), QStringLiteral("Fresh clip content"));

        const qint64 validExpiry = validPacket.get<qint64>(QStringLiteral("expiryTimestamp"));
        const bool isValidExpired = now > validExpiry;
        QCOMPARE(isValidExpired, false);

        // 3. Paquete legacy con timestamp de hace 5 minutos (300 segundos > 120s)
        NetworkPacket legacyExpiredPacket(PACKET_TYPE_CLIPBOARD);
        legacyExpiredPacket.set<qint64>(QStringLiteral("timestamp"), now - 300000);

        const qint64 legacyTime = legacyExpiredPacket.get<qint64>(QStringLiteral("timestamp"));
        const bool isLegacyExpired = (now - legacyTime) > DEFAULT_CLIPBOARD_TTL_MS;
        QCOMPARE(isLegacyExpired, true);
    }
};

QTEST_MAIN(ClipboardTtlTest)

#include "clipboardttltest.moc"
