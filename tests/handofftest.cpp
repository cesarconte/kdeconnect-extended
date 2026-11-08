/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "../plugins/handoff/handoffplugin.h"
#include <QDateTime>
#include <QTest>
#include <QUrl>
#include <core/networkpacket.h>

class HandoffTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testHandoffActivityPacket()
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const QString testUrl = QStringLiteral("https://cachyos.org/documentation");
        const QString testTitle = QStringLiteral("CachyOS Documentation");
        const QString testApp = QStringLiteral("Firefox");

        NetworkPacket np(PACKET_TYPE_HANDOFF_ACTIVITY);
        np.set(QStringLiteral("url"), testUrl);
        np.set(QStringLiteral("title"), testTitle);
        np.set(QStringLiteral("sourceApp"), testApp);
        np.set<qint64>(QStringLiteral("timestamp"), now);

        QCOMPARE(np.type(), PACKET_TYPE_HANDOFF_ACTIVITY);
        QCOMPARE(np.get<QString>(QStringLiteral("url")), testUrl);
        QCOMPARE(np.get<QString>(QStringLiteral("title")), testTitle);
        QCOMPARE(np.get<QString>(QStringLiteral("sourceApp")), testApp);
        QCOMPARE(np.get<qint64>(QStringLiteral("timestamp")), now);

        QUrl parsedUrl = QUrl::fromUserInput(np.get<QString>(QStringLiteral("url")));
        QVERIFY(parsedUrl.isValid());
        QCOMPARE(parsedUrl.host(), QStringLiteral("cachyos.org"));
    }

    void testHandoffClearPacket()
    {
        NetworkPacket clearNp(PACKET_TYPE_HANDOFF_CLEAR);
        QCOMPARE(clearNp.type(), PACKET_TYPE_HANDOFF_CLEAR);
    }

    void testShareRequestUrlPacket()
    {
        const QByteArray testUrl = "https://kde.org/plasma-desktop";
        NetworkPacket np(PACKET_TYPE_SHARE_REQUEST);
        np.set(QStringLiteral("url"), testUrl);

        QCOMPARE(np.type(), PACKET_TYPE_SHARE_REQUEST);
        QUrl parsedUrl = QUrl::fromEncoded(np.get<QByteArray>(QStringLiteral("url")));
        QVERIFY(parsedUrl.isValid());
        QCOMPARE(parsedUrl.scheme(), QStringLiteral("https"));
        QCOMPARE(parsedUrl.host(), QStringLiteral("kde.org"));
    }

    void testShareRequestTextPacket()
    {
        const QString testText = QStringLiteral("https://youtube.com/watch?v=12345");
        NetworkPacket np(PACKET_TYPE_SHARE_REQUEST);
        np.set(QStringLiteral("text"), testText);

        QCOMPARE(np.type(), PACKET_TYPE_SHARE_REQUEST);
        QUrl parsedUrl = QUrl::fromUserInput(np.get<QString>(QStringLiteral("text")));
        QVERIFY(parsedUrl.isValid());
        QCOMPARE(parsedUrl.host(), QStringLiteral("youtube.com"));
    }
};

QTEST_MAIN(HandoffTest)

#include "handofftest.moc"
