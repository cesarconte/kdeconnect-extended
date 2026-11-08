/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "../plugins/continuitymic/continuitymicengine.h"
#include "../plugins/continuitymic/continuitymicplugin.h"
#include <QSignalSpy>
#include <QTest>
#include <core/networkpacket.h>

class ContinuityMicTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testEngineLifecycle()
    {
        ContinuityMicEngine engine;
        QCOMPARE(engine.isRunning(), false);

        QSignalSpy stateSpy(&engine, &ContinuityMicEngine::stateChanged);

        bool started = engine.start(QStringLiteral("Test Mobile Device"));
        if (started) {
            QCOMPARE(engine.isRunning(), true);
            QVERIFY(stateSpy.count() >= 1);

            // Simular un bloque de audio PCM de 10ms a 48kHz estéreo 16-bit (480 muestras * 4 bytes = 1920 bytes)
            QByteArray dummyPcm(1920, 0);
            engine.pushAudio(dummyPcm);

            engine.stop();
            QCOMPARE(engine.isRunning(), false);
        } else {
            // Si el servidor de PipeWire no estuviese activo en el entorno de pruebas, no debe crashear
            QCOMPARE(engine.isRunning(), false);
        }
    }

    void testPacketProtocol()
    {
        NetworkPacket req(PACKET_TYPE_CONTINUITYMIC_REQUEST);
        req.set(QStringLiteral("rate"), 48000);
        req.set(QStringLiteral("channels"), 2);
        req.set(QStringLiteral("format"), QStringLiteral("s16le"));

        QCOMPARE(req.type(), PACKET_TYPE_CONTINUITYMIC_REQUEST);
        QCOMPARE(req.get<int>(QStringLiteral("rate")), 48000);
        QCOMPARE(req.get<int>(QStringLiteral("channels")), 2);
        QCOMPARE(req.get<QString>(QStringLiteral("format")), QStringLiteral("s16le"));

        // Paquete de audio con PCM codificado en Base64
        QByteArray sampleAudio("RAW_PCM_AUDIO_DATA_TEST_12345");
        NetworkPacket audioPacket(PACKET_TYPE_CONTINUITYMIC_AUDIO);
        audioPacket.set(QStringLiteral("pcm"), sampleAudio.toBase64());

        QCOMPARE(audioPacket.type(), PACKET_TYPE_CONTINUITYMIC_AUDIO);
        QByteArray decoded = QByteArray::fromBase64(audioPacket.get<QByteArray>(QStringLiteral("pcm")));
        QCOMPARE(decoded, sampleAudio);
    }
};

QTEST_MAIN(ContinuityMicTest)

#include "continuitymictest.moc"
