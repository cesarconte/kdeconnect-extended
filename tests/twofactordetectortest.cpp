/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>
#include <QObject>
#include <QTest>

#include "twofactordetector.h"

class TwoFactorDetectorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void testSpanishBankSms();
    void testWhatsAppSms();
    void testGoogleVerification();
    void testSteamAndDiscord();
    void testEnglishServices();
    void testNegativeCases();
    void testSecureClipboardCopy();
};

void TwoFactorDetectorTest::initTestCase()
{
}

void TwoFactorDetectorTest::testSpanishBankSms()
{
    // Santander
    QString santanderMsg = QStringLiteral("Santander: Para autorizar tu compra de 45.20 EUR en Amazon introduce la clave 928104. Caduca en 5 min.");
    TwoFactorResult r1 = TwoFactorDetector::detect(santanderMsg, QStringLiteral("Santander"), QStringLiteral("Mensajes"));
    QVERIFY(r1.detected);
    QCOMPARE(r1.code, QStringLiteral("928104"));
    QCOMPARE(r1.service, QStringLiteral("Santander"));

    // BBVA
    QString bbvaMsg = QStringLiteral("BBVA: 382910 es tu clave de acceso para operar.");
    TwoFactorResult r2 = TwoFactorDetector::detect(bbvaMsg, QStringLiteral("BBVA"));
    QVERIFY(r2.detected);
    QCOMPARE(r2.code, QStringLiteral("382910"));
    QCOMPARE(r2.service, QStringLiteral("BBVA"));

    // CaixaBank / Bizum
    QString caixabankMsg = QStringLiteral("CaixaBank: Clave de confirmacion 719203 para emitir Bizum de 20.00 EUR a Carlos.");
    TwoFactorResult r3 = TwoFactorDetector::detect(caixabankMsg);
    QVERIFY(r3.detected);
    QCOMPARE(r3.code, QStringLiteral("719203"));
    QCOMPARE(r3.service, QStringLiteral("CaixaBank"));
}

void TwoFactorDetectorTest::testWhatsAppSms()
{
    QString waMsg = QStringLiteral("Tu código de verificación de WhatsApp es: 482-910. No lo compartas con nadie.");
    TwoFactorResult r = TwoFactorDetector::detect(waMsg, QStringLiteral("WhatsApp"));
    QVERIFY(r.detected);
    QCOMPARE(r.code, QStringLiteral("482910"));
    QCOMPARE(r.service, QStringLiteral("WhatsApp"));
}

void TwoFactorDetectorTest::testGoogleVerification()
{
    QString gMsg = QStringLiteral("G-492019 es tu código de verificación de Google.");
    TwoFactorResult r = TwoFactorDetector::detect(gMsg);
    QVERIFY(r.detected);
    QCOMPARE(r.code, QStringLiteral("G-492019"));
    QCOMPARE(r.service, QStringLiteral("Google"));
}

void TwoFactorDetectorTest::testSteamAndDiscord()
{
    QString steamMsg = QStringLiteral("Tu código de seguridad para Steam Guard es 9XY7K.");
    TwoFactorResult r1 = TwoFactorDetector::detect(steamMsg);
    QVERIFY(r1.detected);
    QCOMPARE(r1.code, QStringLiteral("9XY7K"));
    QCOMPARE(r1.service, QStringLiteral("Steam"));

    QString discordMsg = QStringLiteral("829104 is your verification code for Discord.");
    TwoFactorResult r2 = TwoFactorDetector::detect(discordMsg);
    QVERIFY(r2.detected);
    QCOMPARE(r2.code, QStringLiteral("829104"));
    QCOMPARE(r2.service, QStringLiteral("Discord"));
}

void TwoFactorDetectorTest::testEnglishServices()
{
    QString ghMsg = QStringLiteral("Use 938210 to verify your GitHub identity.");
    TwoFactorResult r = TwoFactorDetector::detect(ghMsg);
    QVERIFY(r.detected);
    QCOMPARE(r.code, QStringLiteral("938210"));
    QCOMPARE(r.service, QStringLiteral("GitHub"));

    QString uberMsg = QStringLiteral("Your Uber code is 4920. Never share this code.");
    TwoFactorResult r2 = TwoFactorDetector::detect(uberMsg);
    QVERIFY(r2.detected);
    QCOMPARE(r2.code, QStringLiteral("4920"));
    QCOMPARE(r2.service, QStringLiteral("Uber"));
}

void TwoFactorDetectorTest::testNegativeCases()
{
    // Casual chat
    QString chatMsg = QStringLiteral("Hola, ¿quedamos mañana a las 14:30 en la calle Mayor 25?");
    TwoFactorResult r1 = TwoFactorDetector::detect(chatMsg);
    QVERIFY(!r1.detected);

    // Tracking / package SMS without OTP keywords
    QString pkgMsg = QStringLiteral("Tu paquete con identificador 94829104 llegará el viernes antes de las 18:00.");
    TwoFactorResult r2 = TwoFactorDetector::detect(pkgMsg);
    QVERIFY(!r2.detected);

    // Casual phone number mention
    QString telMsg = QStringLiteral("Llama a mamá al 612345678 para avisarle.");
    TwoFactorResult r3 = TwoFactorDetector::detect(telMsg);
    QVERIFY(!r3.detected);
}

void TwoFactorDetectorTest::testSecureClipboardCopy()
{
    QString testCode = QStringLiteral("998877");
    bool ok = TwoFactorDetector::copyToClipboardSecurely(testCode, 0); // 0 = no timeout for immediate test
    QVERIFY(ok);

    QClipboard *clip = QGuiApplication::clipboard();
    QVERIFY(clip != nullptr);
    QCOMPARE(clip->text(), testCode);

    const QMimeData *mime = clip->mimeData();
    QVERIFY(mime != nullptr);
    QVERIFY(mime->hasFormat(QStringLiteral("x-kde-passwordManagerHint")));
    QCOMPARE(mime->data(QStringLiteral("x-kde-passwordManagerHint")), QByteArrayLiteral("secret"));
}

QTEST_MAIN(TwoFactorDetectorTest)
#include "twofactordetectortest.moc"
