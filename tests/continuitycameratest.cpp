/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "../plugins/continuitycamera/continuitycameraengine.h"
#include "../plugins/continuitycamera/continuitycameraplugin.h"
#include <QBuffer>
#include <QImage>
#include <QSignalSpy>
#include <QTest>
#include <core/networkpacket.h>

class ContinuityCameraTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testEngineLifecycle()
    {
        ContinuityCameraEngine engine;
        QCOMPARE(engine.isRunning(), false);

        QSignalSpy stateSpy(&engine, &ContinuityCameraEngine::stateChanged);

        bool started = engine.start(QStringLiteral("Test Mobile Camera"), 640, 480);
        if (started) {
            QCOMPARE(engine.isRunning(), true);
            QVERIFY(stateSpy.count() >= 1);

            // Crear y enviar frame QImage sintético
            QImage frame(640, 480, QImage::Format_RGBA8888);
            frame.fill(Qt::blue);
            engine.pushFrame(frame);

            // Generar y enviar payload comprimido PNG/JPEG
            QByteArray imgBytes;
            QBuffer buffer(&imgBytes);
            buffer.open(QIODevice::WriteOnly);
            frame.save(&buffer, "PNG");
            engine.pushJpegData(imgBytes);

            engine.stop();
            QCOMPARE(engine.isRunning(), false);
        } else {
            // Manejo preventivo si no hay daemon PipeWire en entorno CI estéril
            QCOMPARE(engine.isRunning(), false);
        }
    }

    void testPacketProtocol()
    {
        NetworkPacket req(PACKET_TYPE_CONTINUITYCAMERA_REQUEST);
        req.set(QStringLiteral("width"), 1280);
        req.set(QStringLiteral("height"), 720);
        req.set(QStringLiteral("fps"), 30);

        QCOMPARE(req.type(), PACKET_TYPE_CONTINUITYCAMERA_REQUEST);
        QCOMPARE(req.get<int>(QStringLiteral("width")), 1280);
        QCOMPARE(req.get<int>(QStringLiteral("height")), 720);
        QCOMPARE(req.get<int>(QStringLiteral("fps")), 30);

        QByteArray dummyPayload("DUMMY_JPEG_CAMERA_STREAM_BYTES");
        NetworkPacket framePacket(PACKET_TYPE_CONTINUITYCAMERA_FRAME);
        framePacket.set(QStringLiteral("jpeg"), dummyPayload.toBase64());

        QCOMPARE(framePacket.type(), PACKET_TYPE_CONTINUITYCAMERA_FRAME);
        QByteArray decoded = QByteArray::fromBase64(framePacket.get<QByteArray>(QStringLiteral("jpeg")));
        QCOMPARE(decoded, dummyPayload);
    }
};

QTEST_MAIN(ContinuityCameraTest)

#include "continuitycameratest.moc"
