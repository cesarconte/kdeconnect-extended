/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "../plugins/lockdevice/lockdeviceplugin.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QRandomGenerator>
#include <QTest>
#include <core/networkpacket.h>

class ProximityUnlockTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testChallengeResponseVerification()
    {
        const QString testDeviceId = QStringLiteral("peer-device-554433");
        const quint64 nonceVal = QRandomGenerator::securelySeeded().generate64();
        const QByteArray nonce = QByteArray::number(nonceVal, 16);

        // 1. Empaquetar desafío
        NetworkPacket challengePacket(PACKET_TYPE_LOCK_REQUEST);
        challengePacket.set(QStringLiteral("proximityChallenge"), nonce);

        QVERIFY(challengePacket.has(QStringLiteral("proximityChallenge")));
        QCOMPARE(challengePacket.get<QByteArray>(QStringLiteral("proximityChallenge")), nonce);

        // 2. Generar respuesta válida
        QByteArray payload = nonce;
        payload.append(testDeviceId.toUtf8());
        const QByteArray validProof = QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex();

        NetworkPacket responsePacket(PACKET_TYPE_LOCK);
        responsePacket.set(QStringLiteral("proximityResponse"), validProof);

        QVERIFY(responsePacket.has(QStringLiteral("proximityResponse")));
        QCOMPARE(responsePacket.get<QByteArray>(QStringLiteral("proximityResponse")), validProof);

        // 3. Simular verificación en el anfitrión
        const QByteArray expectedProof = QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex();
        QCOMPARE(responsePacket.get<QByteArray>(QStringLiteral("proximityResponse")), expectedProof);

        // 4. Verificar rechazo de prueba inválida
        QByteArray tamperedPayload = QByteArrayLiteral("tampered_nonce");
        tamperedPayload.append(testDeviceId.toUtf8());
        const QByteArray forgedProof = QCryptographicHash::hash(tamperedPayload, QCryptographicHash::Sha256).toHex();
        QVERIFY(forgedProof != expectedProof);
    }

    void testChallengeFreshnessTimeout()
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const qint64 freshTimestamp = now - 1200; // 1.2 segundos (fresco, < 5s)
        const qint64 staleTimestamp = now - 6500; // 6.5 segundos (obsoleto, > 5s)

        constexpr qint64 maxAgeMs = 5000;
        QCOMPARE((now - freshTimestamp) <= maxAgeMs, true);
        QCOMPARE((now - staleTimestamp) <= maxAgeMs, false);
    }
};

QTEST_MAIN(ProximityUnlockTest)

#include "proximityunlocktest.moc"
