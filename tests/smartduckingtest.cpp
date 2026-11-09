/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include <QSignalSpy>
#include <QTest>
#include <core/networkpacket.h>

class SmartDuckingTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testTelephonyRingingPacket()
    {
        NetworkPacket np(QStringLiteral("kdeconnect.telephony"));
        np.set(QStringLiteral("event"), QStringLiteral("ringing"));
        np.set(QStringLiteral("phoneNumber"), QStringLiteral("+34600000000"));
        np.set(QStringLiteral("contactName"), QStringLiteral("Test Caller"));

        QCOMPARE(np.type(), QStringLiteral("kdeconnect.telephony"));
        QCOMPARE(np.get<QString>(QStringLiteral("event")), QStringLiteral("ringing"));
        QCOMPARE(np.get<QString>(QStringLiteral("phoneNumber")), QStringLiteral("+34600000000"));
        QCOMPARE(np.get<QString>(QStringLiteral("contactName")), QStringLiteral("Test Caller"));
    }

    void testTelephonyTalkingPacket()
    {
        NetworkPacket np(QStringLiteral("kdeconnect.telephony"));
        np.set(QStringLiteral("event"), QStringLiteral("talking"));

        QCOMPARE(np.type(), QStringLiteral("kdeconnect.telephony"));
        QCOMPARE(np.get<QString>(QStringLiteral("event")), QStringLiteral("talking"));
        QVERIFY(!np.get<bool>(QStringLiteral("isCancel"), false));
    }

    void testTelephonyCancelPacket()
    {
        NetworkPacket np(QStringLiteral("kdeconnect.telephony"));
        np.set(QStringLiteral("isCancel"), true);

        QCOMPARE(np.type(), QStringLiteral("kdeconnect.telephony"));
        QVERIFY(np.get<bool>(QStringLiteral("isCancel")));
    }

    void testDuckingInterpolationCurve()
    {
        qint64 originalVol = 65536; // 100% volume
        int duckPercent = 15;
        qint64 targetVol = (originalVol * duckPercent) / 100;

        // Test math at t = 0.0, 0.5, 1.0
        auto calcVol = [=](qreal t) -> qint64 {
            qreal ease = 1.0 - (1.0 - t) * (1.0 - t);
            return originalVol + static_cast<qint64>((targetVol - originalVol) * ease);
        };

        QCOMPARE(calcVol(0.0), originalVol);
        QCOMPARE(calcVol(1.0), targetVol);
        QVERIFY(calcVol(0.5) < originalVol);
        QVERIFY(calcVol(0.5) > targetVol);
    }
};

QTEST_MAIN(SmartDuckingTest)

#include "smartduckingtest.moc"
