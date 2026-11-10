/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "../plugins/clipboard/clipboardplugin.h"
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

class ClipboardCleanTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testCleanUtmParameters()
    {
        QUrl dirty(
            QStringLiteral("https://example.com/article?utm_source=twitter&utm_medium=social&utm_campaign=launch&utm_term=kde&utm_content=v1&post_id=42"));
        int removed = 0;
        QUrl clean = ClipboardPlugin::cleanSingleUrl(dirty, removed);

        QCOMPARE(removed, 5);
        QCOMPARE(clean.toString(), QStringLiteral("https://example.com/article?post_id=42"));
    }

    void testCleanYouTubeTrackers()
    {
        // 1. YouTube share link with si
        QUrl ytShare(QStringLiteral("https://youtu.be/dQw4w9WgXcQ?si=abcdef123456"));
        int removed = 0;
        QUrl cleanShare = ClipboardPlugin::cleanSingleUrl(ytShare, removed);

        QCOMPARE(removed, 1);
        QCOMPARE(cleanShare.toString(), QStringLiteral("https://youtu.be/dQw4w9WgXcQ"));

        // 2. YouTube watch link with v and feature
        QUrl ytWatch(QStringLiteral("https://www.youtube.com/watch?v=dQw4w9WgXcQ&feature=share&si=trk999"));
        removed = 0;
        QUrl cleanWatch = ClipboardPlugin::cleanSingleUrl(ytWatch, removed);

        QCOMPARE(removed, 2);
        QCOMPARE(cleanWatch.toString(), QStringLiteral("https://www.youtube.com/watch?v=dQw4w9WgXcQ"));
    }

    void testCleanInstagramTrackers()
    {
        QUrl insta(QStringLiteral("https://www.instagram.com/reel/DE-XYZ123/?igsh=MWx5bXFia3Jk=="));
        int removed = 0;
        QUrl clean = ClipboardPlugin::cleanSingleUrl(insta, removed);

        QCOMPARE(removed, 1);
        QCOMPARE(clean.toString(), QStringLiteral("https://www.instagram.com/reel/DE-XYZ123/"));
    }

    void testCleanTwitterTrackers()
    {
        QUrl tweet(QStringLiteral("https://x.com/kdecommunity/status/1234567890?s=20&t=abcdef"));
        int removed = 0;
        QUrl clean = ClipboardPlugin::cleanSingleUrl(tweet, removed);

        QCOMPARE(removed, 2);
        QCOMPARE(clean.toString(), QStringLiteral("https://x.com/kdecommunity/status/1234567890"));
    }

    void testCleanAmazonTrackers()
    {
        QUrl amazon(QStringLiteral("https://www.amazon.es/dp/B08N5WRWNW?ref_=cm_sw_r_cp_ud_dp_123&tag=affil-21"));
        int removed = 0;
        QUrl clean = ClipboardPlugin::cleanSingleUrl(amazon, removed);

        QCOMPARE(removed, 2);
        QCOMPARE(clean.toString(), QStringLiteral("https://www.amazon.es/dp/B08N5WRWNW"));
    }

    void testPreserveLegitimateParameters()
    {
        QUrl search(QStringLiteral("https://www.google.com/search?q=kde+connect+plasma&hl=es&page=2"));
        int removed = 0;
        QUrl clean = ClipboardPlugin::cleanSingleUrl(search, removed);

        QCOMPARE(removed, 0);
        QCOMPARE(clean.toString(), QStringLiteral("https://www.google.com/search?q=kde+connect+plasma&hl=es&page=2"));
    }

    void testSanitizeTrackingInText()
    {
        QString text = QStringLiteral("Mira este video: https://youtu.be/dQw4w9WgXcQ?si=trk999, está genial! Y revisa https://instagram.com/p/123?igsh=test.");
        bool wasSanitized = false;
        int strippedCount = 0;
        QString result = ClipboardPlugin::sanitizeTracking(text, wasSanitized, strippedCount);

        QVERIFY(wasSanitized);
        QCOMPARE(strippedCount, 2);
        QCOMPARE(result, QStringLiteral("Mira este video: https://youtu.be/dQw4w9WgXcQ, está genial! Y revisa https://instagram.com/p/123."));
    }

    void testOcrExtraction()
    {
        QString tesseractBin = QStandardPaths::findExecutable(QStringLiteral("tesseract"));
        if (tesseractBin.isEmpty() && QFileInfo::exists(QStringLiteral("/usr/bin/tesseract"))) {
            tesseractBin = QStringLiteral("/usr/bin/tesseract");
        }

        if (tesseractBin.isEmpty()) {
            QSKIP("Tesseract is not installed on this system");
        }

        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QString imgPath = tmpDir.filePath(QStringLiteral("ocr_test.png"));

        QImage img(400, 100, QImage::Format_RGB32);
        img.fill(Qt::white);
        QPainter painter(&img);
        painter.setPen(Qt::black);
        QFont font = painter.font();
        font.setPixelSize(28);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(20, 60, QStringLiteral("KDE Connect OCR Test"));
        painter.end();
        QVERIFY(img.save(imgPath));

        QProcess proc;
        proc.start(tesseractBin,
                   {imgPath,
                    QStringLiteral("stdout"),
                    QStringLiteral("--oem"),
                    QStringLiteral("1"),
                    QStringLiteral("-l"),
                    QStringLiteral("eng"),
                    QStringLiteral("--loglevel"),
                    QStringLiteral("ERROR")});
        QVERIFY(proc.waitForFinished(5000));
        QCOMPARE(proc.exitCode(), 0);

        QString ocrText = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        QVERIFY(ocrText.contains(QStringLiteral("KDE Connect")));
    }

    void testFacebookAndGoogleClickIds()
    {
        QUrl url(QStringLiteral("https://shop.example.com/item?gclid=ABC123xyz&fbclid=FB987654&wbraid=WB555&gbraid=GB666&item_id=77"));
        int removed = 0;
        QUrl clean = ClipboardPlugin::cleanSingleUrl(url, removed);

        QCOMPARE(removed, 4);
        QCOMPARE(clean.toString(), QStringLiteral("https://shop.example.com/item?item_id=77"));
    }

    void testAliExpressTrackers()
    {
        QUrl url(QStringLiteral("https://es.aliexpress.com/item/100500123.html?spm=a2g0o.productlist.main.1&scm=1007.40000.123"));
        int removed = 0;
        QUrl clean = ClipboardPlugin::cleanSingleUrl(url, removed);

        QCOMPARE(removed, 2);
        QCOMPARE(clean.toString(), QStringLiteral("https://es.aliexpress.com/item/100500123.html"));
    }

    void testStripFormatting()
    {
        ClipboardListener::instance()->setText(QStringLiteral("Texto en formato plano"));
        QCOMPARE(ClipboardListener::instance()->currentContent().toString(), QStringLiteral("Texto en formato plano"));
    }
};

QTEST_MAIN(ClipboardCleanTest)

#include "clipboardcleantest.moc"
