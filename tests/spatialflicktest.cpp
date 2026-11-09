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

class SpatialFlickTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testFlickApiEndpoint()
    {
        DeckServer server(54345);
        QVERIFY(server.start());

        QSignalSpy spy(&server, &DeckServer::flickReceived);

        QTcpSocket client;
        client.connectToHost(QStringLiteral("127.0.0.1"), 54345);
        QVERIFY(client.waitForConnected(2000));
        QTest::qWait(50);

        QJsonObject flickObj;
        flickObj[QStringLiteral("filename")] = QStringLiteral("photo_vacation.jpg");
        flickObj[QStringLiteral("data")] = QStringLiteral("aGVsbG8gd29ybGQ="); // base64 "hello world"
        flickObj[QStringLiteral("text")] = QString();
        flickObj[QStringLiteral("action")] = QStringLiteral("flick");

        QByteArray body = QJsonDocument(flickObj).toJson(QJsonDocument::Compact);
        QByteArray req =
            "POST /api/flick HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: "
            + QByteArray::number(body.size()) + "\r\n\r\n" + body;

        client.write(req);
        client.flush();
        QTest::qWait(50);

        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);
        QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("photo_vacation.jpg"));
        QCOMPARE(args.at(1).toString(), QStringLiteral("aGVsbG8gd29ybGQ="));
        QCOMPARE(args.at(2).toString(), QString());
        QCOMPARE(args.at(3).toString(), QStringLiteral("flick"));

        server.stop();
    }

    void testFlickApiClipboard()
    {
        DeckServer server(54346);
        QVERIFY(server.start());

        QSignalSpy spy(&server, &DeckServer::flickReceived);

        QTcpSocket client;
        client.connectToHost(QStringLiteral("127.0.0.1"), 54346);
        QVERIFY(client.waitForConnected(2000));
        QTest::qWait(50);

        QJsonObject flickObj;
        flickObj[QStringLiteral("filename")] = QStringLiteral("clipboard.txt");
        flickObj[QStringLiteral("data")] = QString();
        flickObj[QStringLiteral("text")] = QStringLiteral("https://kde.org/plasma-desktop");
        flickObj[QStringLiteral("action")] = QStringLiteral("flick");

        QByteArray body = QJsonDocument(flickObj).toJson(QJsonDocument::Compact);
        QByteArray req =
            "POST /api/flick HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: "
            + QByteArray::number(body.size()) + "\r\n\r\n" + body;

        client.write(req);
        client.flush();
        QTest::qWait(50);

        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);
        QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("clipboard.txt"));
        QCOMPARE(args.at(1).toString(), QString());
        QCOMPARE(args.at(2).toString(), QStringLiteral("https://kde.org/plasma-desktop"));
        QCOMPARE(args.at(3).toString(), QStringLiteral("flick"));

        server.stop();
    }
};

QTEST_MAIN(SpatialFlickTest)

#include "spatialflicktest.moc"
