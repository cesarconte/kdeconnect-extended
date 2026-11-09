/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "../plugins/biometricauth/biometricauthplugin.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>
#include <core/networkpacket.h>

class BiometricAuthTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testBiometricRequestPacket()
    {
        const QString transactionId = QStringLiteral("test-uuid-12345");
        const QString user = QStringLiteral("cesar");
        const QString service = QStringLiteral("sudo");
        const QString hostname = QStringLiteral("cachyos-desktop");
        const qint64 now = QDateTime::currentMSecsSinceEpoch();

        NetworkPacket np(PACKET_TYPE_BIOMETRIC_REQUEST);
        np.set(QStringLiteral("transactionId"), transactionId);
        np.set(QStringLiteral("user"), user);
        np.set(QStringLiteral("service"), service);
        np.set(QStringLiteral("hostname"), hostname);
        np.set<qint64>(QStringLiteral("timestamp"), now);

        QCOMPARE(np.type(), PACKET_TYPE_BIOMETRIC_REQUEST);
        QCOMPARE(np.get<QString>(QStringLiteral("transactionId")), transactionId);
        QCOMPARE(np.get<QString>(QStringLiteral("user")), user);
        QCOMPARE(np.get<QString>(QStringLiteral("service")), service);
        QCOMPARE(np.get<QString>(QStringLiteral("hostname")), hostname);
        QCOMPARE(np.get<qint64>(QStringLiteral("timestamp")), now);
    }

    void testBiometricResponsePacketAccepted()
    {
        const QString transactionId = QStringLiteral("test-uuid-12345");
        NetworkPacket np(PACKET_TYPE_BIOMETRIC_RESPONSE);
        np.set(QStringLiteral("transactionId"), transactionId);
        np.set(QStringLiteral("accepted"), true);

        QCOMPARE(np.type(), PACKET_TYPE_BIOMETRIC_RESPONSE);
        QCOMPARE(np.get<QString>(QStringLiteral("transactionId")), transactionId);
        QVERIFY(np.get<bool>(QStringLiteral("accepted")));
    }

    void testBiometricResponsePacketRejected()
    {
        const QString transactionId = QStringLiteral("test-uuid-12345");
        NetworkPacket np(PACKET_TYPE_BIOMETRIC_RESPONSE);
        np.set(QStringLiteral("transactionId"), transactionId);
        np.set(QStringLiteral("accepted"), false);

        QCOMPARE(np.type(), PACKET_TYPE_BIOMETRIC_RESPONSE);
        QCOMPARE(np.get<QString>(QStringLiteral("transactionId")), transactionId);
        QVERIFY(!np.get<bool>(QStringLiteral("accepted")));
    }

    void testPamProtocolJsonFormat()
    {
        // Simular solicitud PAM
        QJsonObject req;
        req[QStringLiteral("action")] = QStringLiteral("authenticate");
        req[QStringLiteral("user")] = QStringLiteral("cesar");
        req[QStringLiteral("service")] = QStringLiteral("sudo");

        const QByteArray reqBytes = QJsonDocument(req).toJson(QJsonDocument::Compact);
        QVERIFY(reqBytes.contains("\"action\":\"authenticate\""));
        QVERIFY(reqBytes.contains("\"user\":\"cesar\""));
        QVERIFY(reqBytes.contains("\"service\":\"sudo\""));

        // Simular respuesta exitosa del daemon
        QJsonObject resp;
        resp[QStringLiteral("status")] = QStringLiteral("success");
        resp[QStringLiteral("message")] = QStringLiteral("Autenticado con éxito");

        const QByteArray respBytes = QJsonDocument(resp).toJson(QJsonDocument::Compact);
        QVERIFY(respBytes.contains("\"status\":\"success\""));
    }

    void testDualKeyRequestPacket()
    {
        const QString transactionId = QStringLiteral("dual-key-test-uuid");
        const QString user = QStringLiteral("cesar");
        const QString service = QStringLiteral("sudo");
        const QString command = QStringLiteral("dd if=/dev/zero of=/dev/sdb bs=1M");
        const QString riskLevel = QStringLiteral("critical");
        const QString hostname = QStringLiteral("cachyos-desktop");
        const qint64 now = QDateTime::currentMSecsSinceEpoch();

        NetworkPacket np(PACKET_TYPE_BIOMETRIC_REQUEST);
        np.set(QStringLiteral("transactionId"), transactionId);
        np.set(QStringLiteral("user"), user);
        np.set(QStringLiteral("service"), service);
        np.set(QStringLiteral("command"), command);
        np.set(QStringLiteral("riskLevel"), riskLevel);
        np.set(QStringLiteral("hostname"), hostname);
        np.set<qint64>(QStringLiteral("timestamp"), now);

        QCOMPARE(np.type(), PACKET_TYPE_BIOMETRIC_REQUEST);
        QCOMPARE(np.get<QString>(QStringLiteral("transactionId")), transactionId);
        QCOMPARE(np.get<QString>(QStringLiteral("user")), user);
        QCOMPARE(np.get<QString>(QStringLiteral("service")), service);
        QCOMPARE(np.get<QString>(QStringLiteral("command")), command);
        QCOMPARE(np.get<QString>(QStringLiteral("riskLevel")), riskLevel);
    }

    void testDualKeyPamProtocolWithCommand()
    {
        QJsonObject req;
        req[QStringLiteral("action")] = QStringLiteral("authenticate");
        req[QStringLiteral("user")] = QStringLiteral("cesar");
        req[QStringLiteral("service")] = QStringLiteral("sudo");
        req[QStringLiteral("command")] = QStringLiteral("wipefs -a /dev/nvme0n1");

        const QByteArray reqBytes = QJsonDocument(req).toJson(QJsonDocument::Compact);
        QVERIFY(reqBytes.contains("\"command\":\"wipefs -a /dev/nvme0n1\""));
    }
};

QTEST_MAIN(BiometricAuthTest)

#include "biometricauthtest.moc"
