/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "../plugins/plasmadeck/deckserver.h"
#include "../plugins/spatialpresence/spatialpresenceplugin.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTest>
#include <core/networkpacket.h>

class SpatialPresenceTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testGesturePacketSerialization()
    {
        const QStringList gestures = {
            QStringLiteral("pointing_right"),
            QStringLiteral("pointing_left"),
            QStringLiteral("thumb_up"),
            QStringLiteral("thumb_down"),
            QStringLiteral("palm_open"),
            QStringLiteral("fist_hold"),
            QStringLiteral("wave_right"),
            QStringLiteral("wave_left"),
            QStringLiteral("palm_hold"),
            QStringLiteral("wave_up"),
            QStringLiteral("wave_down"),
        };

        for (const QString &g : gestures) {
            NetworkPacket np(PACKET_TYPE_SPATIAL_PRESENCE, {{QStringLiteral("gesture"), g}});
            QCOMPARE(np.type(), PACKET_TYPE_SPATIAL_PRESENCE);
            QVERIFY(np.has(QStringLiteral("gesture")));
            QCOMPARE(np.get<QString>(QStringLiteral("gesture")), g);
        }
    }

    void testPresencePacketSerialization()
    {
        NetworkPacket np(PACKET_TYPE_SPATIAL_PRESENCE,
                         {
                             {QStringLiteral("presence"), QStringLiteral("absent")},
                             {QStringLiteral("durationMs"), 15000},
                         });

        QCOMPARE(np.type(), PACKET_TYPE_SPATIAL_PRESENCE);
        QVERIFY(np.has(QStringLiteral("presence")));
        QCOMPARE(np.get<QString>(QStringLiteral("presence")), QStringLiteral("absent"));
        QCOMPARE(np.get<int>(QStringLiteral("durationMs")), 15000);

        NetworkPacket npPresent(PACKET_TYPE_SPATIAL_PRESENCE,
                                {
                                    {QStringLiteral("presence"), QStringLiteral("present")},
                                    {QStringLiteral("durationMs"), 0},
                                });
        QCOMPARE(npPresent.get<QString>(QStringLiteral("presence")), QStringLiteral("present"));
        QCOMPARE(npPresent.get<int>(QStringLiteral("durationMs")), 0);
    }

    void testDeckServerSpatialEndpoint()
    {
        DeckServer server(54345);
        QVERIFY(server.start());

        QSignalSpy spy(&server, &DeckServer::spatialEventReceived);

        QTcpSocket client;
        client.connectToHost(QStringLiteral("127.0.0.1"), 54345);
        QVERIFY(client.waitForConnected(2000));
        QTest::qWait(50);

        // Enviar gesto wave_right
        QJsonObject gestureObj;
        gestureObj[QStringLiteral("type")] = QStringLiteral("gesture");
        gestureObj[QStringLiteral("gesture")] = QStringLiteral("wave_right");
        QByteArray body = QJsonDocument(gestureObj).toJson(QJsonDocument::Compact);

        QByteArray req =
            "POST /api/spatial HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: "
            + QByteArray::number(body.size()) + "\r\n\r\n" + body;

        client.write(req);
        client.flush();

        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);
        QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("gesture"));
        QCOMPARE(args.at(1).toString(), QStringLiteral("wave_right"));
        QCOMPARE(args.at(2).toInt(), 0);

        // Enviar evento de presencia
        QTcpSocket client2;
        client2.connectToHost(QStringLiteral("127.0.0.1"), 54345);
        QVERIFY(client2.waitForConnected(2000));
        QTest::qWait(50);

        QJsonObject presenceObj;
        presenceObj[QStringLiteral("type")] = QStringLiteral("presence");
        presenceObj[QStringLiteral("state")] = QStringLiteral("absent");
        presenceObj[QStringLiteral("durationMs")] = 15000;
        QByteArray body2 = QJsonDocument(presenceObj).toJson(QJsonDocument::Compact);

        QByteArray req2 =
            "POST /api/spatial HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: "
            + QByteArray::number(body2.size()) + "\r\n\r\n" + body2;

        client2.write(req2);
        client2.flush();

        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);
        QList<QVariant> args2 = spy.takeFirst();
        QCOMPARE(args2.at(0).toString(), QStringLiteral("presence"));
        QCOMPARE(args2.at(1).toString(), QStringLiteral("absent"));
        QCOMPARE(args2.at(2).toInt(), 15000);

        server.stop();
    }
};

QTEST_MAIN(SpatialPresenceTest)

#include "spatialpresencetest.moc"
