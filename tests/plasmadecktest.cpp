/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "../plugins/plasmadeck/deckserver.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTest>

class PlasmaDeckTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testServerStartAndStop()
    {
        DeckServer server(54329); // Test port
        QVERIFY(server.start());
        QCOMPARE(server.port(), static_cast<quint16>(54329));
        QCOMPARE(server.serverUrl(QStringLiteral("192.168.1.100")), QStringLiteral("http://192.168.1.100:54329/"));
        server.stop();
    }

    void testHttpGetIndexHtml()
    {
        DeckServer server(54330);
        QVERIFY(server.start());

        QTcpSocket client;
        client.connectToHost(QStringLiteral("127.0.0.1"), 54330);
        QVERIFY(client.waitForConnected(2000));
        QTest::qWait(50);

        client.write("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
        client.flush();
        QTest::qWait(50);

        QTRY_VERIFY_WITH_TIMEOUT(client.bytesAvailable() > 0, 2000);
        QByteArray resp = client.readAll();
        QVERIFY(resp.startsWith("HTTP/1.1 200 OK"));
        QVERIFY(resp.contains("<title>Plasma Deck</title>"));
        QVERIFY(resp.contains("deck-grid"));

        server.stop();
    }

    void testActionTrigger()
    {
        DeckServer server(54331);
        QVERIFY(server.start());

        QSignalSpy spy(&server, &DeckServer::actionTriggered);

        QTcpSocket client;
        client.connectToHost(QStringLiteral("127.0.0.1"), 54331);
        QVERIFY(client.waitForConnected(2000));
        QTest::qWait(50);

        QJsonObject actionObj;
        actionObj[QStringLiteral("id")] = QStringLiteral("screenshot");
        actionObj[QStringLiteral("command")] = QStringLiteral("spectacle");
        QByteArray body = QJsonDocument(actionObj).toJson(QJsonDocument::Compact);

        QByteArray req = "POST /api/action HTTP/1.1\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
        client.write(req);
        client.flush();
        QTest::qWait(50);

        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);
        QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("screenshot"));
        QCOMPARE(args.at(1).toString(), QStringLiteral("spectacle"));

        server.stop();
    }
};

QTEST_MAIN(PlasmaDeckTest)

#include "plasmadecktest.moc"
