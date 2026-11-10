/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "twofactordetector.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>
#include <QRegularExpression>
#include <QTimer>

bool TwoFactorDetector::isKeywordPresent(const QString &lowerText)
{
    static const QRegularExpression keywordRegex(QStringLiteral("(c[oó]digo|verif|seguridad|clave|acceso|confirmaci[oó]n|autorizaci[oó]n|"
                                                                "autentic|un solo uso|code|security|otp|2fa|passcode|one-time|pin|sign-in|auth)"),
                                                 QRegularExpression::CaseInsensitiveOption);

    return lowerText.contains(keywordRegex);
}

QString TwoFactorDetector::extractService(const QString &text, const QString &title, const QString &appName)
{
    static const QStringList knownServices = {
        QStringLiteral("WhatsApp"),  QStringLiteral("Google"),   QStringLiteral("Microsoft"), QStringLiteral("Apple"),    QStringLiteral("Amazon"),
        QStringLiteral("Telegram"),  QStringLiteral("Steam"),    QStringLiteral("Discord"),   QStringLiteral("GitHub"),   QStringLiteral("GitLab"),
        QStringLiteral("PayPal"),    QStringLiteral("Bizum"),    QStringLiteral("Twitter"),   QStringLiteral("Facebook"), QStringLiteral("Instagram"),
        QStringLiteral("TikTok"),    QStringLiteral("Uber"),     QStringLiteral("Santander"), QStringLiteral("BBVA"),     QStringLiteral("CaixaBank"),
        QStringLiteral("Bankinter"), QStringLiteral("Sabadell"), QStringLiteral("Openbank"),  QStringLiteral("ING"),      QStringLiteral("Revolut"),
        QStringLiteral("N26"),       QStringLiteral("Abanca"),   QStringLiteral("Kutxabank"), QStringLiteral("Unicaja"),  QStringLiteral("Stripe"),
        QStringLiteral("Binance"),   QStringLiteral("Coinbase")};

    // 1. Prioridad: el remitente explícito en appName o title (aparición más temprana)
    const QString senderContext = appName + QStringLiteral(" ") + title;
    int bestPos = -1;
    QString bestSvc;

    for (const auto &svc : knownServices) {
        int pos = senderContext.indexOf(svc, 0, Qt::CaseInsensitive);
        if (pos != -1 && (bestPos == -1 || pos < bestPos)) {
            bestPos = pos;
            bestSvc = svc;
        }
    }
    if (!bestSvc.isEmpty()) {
        return bestSvc;
    }

    // 2. Si no está en el remitente, buscar el servicio mencionado primero en el texto
    bestPos = -1;
    for (const auto &svc : knownServices) {
        int pos = text.indexOf(svc, 0, Qt::CaseInsensitive);
        if (pos != -1 && (bestPos == -1 || pos < bestPos)) {
            bestPos = pos;
            bestSvc = svc;
        }
    }
    if (!bestSvc.isEmpty()) {
        return bestSvc;
    }

    // Heurística: "para tu cuenta de <Servicio>" o "from <Service>"
    static const QRegularExpression svcRegex(QStringLiteral("(?:para|de|from|for)\\s+(?:tu\\s+cuenta\\s+(?:de\\s+)?)?([A-Z][a-zA-Z0-9_]{2,20})"),
                                             QRegularExpression::CaseInsensitiveOption);
    auto m = svcRegex.match(text);
    if (m.hasMatch()) {
        const QString cand = m.captured(1).trimmed();
        if (cand.compare(QLatin1String("iniciar"), Qt::CaseInsensitive) != 0 && cand.compare(QLatin1String("acceder"), Qt::CaseInsensitive) != 0
            && cand.compare(QLatin1String("confirmar"), Qt::CaseInsensitive) != 0 && cand.compare(QLatin1String("operar"), Qt::CaseInsensitive) != 0) {
            return cand;
        }
    }

    if (!appName.isEmpty() && appName != QStringLiteral("Mensajes") && appName != QStringLiteral("Messages") && appName != QStringLiteral("KDE Connect")
        && appName != QStringLiteral("SMS")) {
        return appName;
    }

    return QString();
}

TwoFactorResult TwoFactorDetector::detect(const QString &text, const QString &title, const QString &appName)
{
    TwoFactorResult result;
    const QString combined = (title.isEmpty() ? QString() : (title + QStringLiteral(" "))) + text;
    if (combined.trimmed().isEmpty()) {
        return result;
    }

    const QString lower = combined.toLower();

    // 1. Verificar si existen palabras clave de verificación
    const bool hasKeywords = isKeywordPresent(lower);
    if (!hasKeywords) {
        // Si no hay palabras clave de verificación, no es un 2FA
        return result;
    }

    // 2. Extraer prefijos estándar alfanuméricos (ej. G-123456, FB-12345)
    static const QRegularExpression prefixedRegex(QStringLiteral("\\b([A-Z]{1,4}-\\d{4,8})\\b"), QRegularExpression::CaseInsensitiveOption);
    auto pMatch = prefixedRegex.match(combined);
    if (pMatch.hasMatch()) {
        result.detected = true;
        result.code = pMatch.captured(1).toUpper();
        result.originalMatch = pMatch.captured(0);
        result.service = extractService(combined, title, appName);
        return result;
    }

    // 3. Patrón explícito contextual: "código [para X] es 123456", "clave de acceso: 123456"
    static const QRegularExpression explicitRegex(
        QStringLiteral("\\b(?:c[oó]digo|clave|pin|otp|passcode)(?:[^\n\r:.,]{0,35}?)\\s*(?:is|es|:)\\s*[:=\\-]?\\s*([A-Za-z0-9]{4,8})\\b"),
        QRegularExpression::CaseInsensitiveOption);
    auto eMatch = explicitRegex.match(combined);
    if (eMatch.hasMatch()) {
        const QString cand = eMatch.captured(1).trimmed();
        // Verificar que no sea una palabra común
        int digits = 0;
        for (const QChar &c : cand) {
            if (c.isDigit())
                digits++;
        }
        if (digits >= 1 && cand.length() >= 4) {
            result.detected = true;
            result.code = cand;
            result.originalMatch = eMatch.captured(0);
            result.service = extractService(combined, title, appName);
            return result;
        }
    }

    // 4. Patrón de acción directa: "use/usa/utiliza 123456 para...", "introduce la clave 123456"
    static const QRegularExpression actionRegex(
        QStringLiteral("\\b(?:use|usa|utiliza|introduce|ingresa|enter)\\s+(?:el\\s+c[oó]digo\\s+|la\\s+clave\\s+)?([A-Za-z0-9]{4,8})\\b"),
        QRegularExpression::CaseInsensitiveOption);
    auto aMatch = actionRegex.match(combined);
    if (aMatch.hasMatch()) {
        const QString cand = aMatch.captured(1).trimmed();
        int digits = 0;
        for (const QChar &c : cand) {
            if (c.isDigit())
                digits++;
        }
        if (digits >= 2) {
            result.detected = true;
            result.code = cand;
            result.originalMatch = aMatch.captured(0);
            result.service = extractService(combined, title, appName);
            return result;
        }
    }

    // 5. Patrón inverso: "123456 es tu código", "123456 is your code"
    static const QRegularExpression reverseRegex(QStringLiteral("\\b([A-Za-z0-9]{4,8})\\s*(?:es\\s+tu\\s+c[oó]digo|is\\s+your\\s+(?:[a-zA-Z0-9_]+\\s+)?code)"),
                                                 QRegularExpression::CaseInsensitiveOption);
    auto rMatch = reverseRegex.match(combined);
    if (rMatch.hasMatch()) {
        result.detected = true;
        result.code = rMatch.captured(1).trimmed();
        result.originalMatch = rMatch.captured(0);
        result.service = extractService(combined, title, appName);
        return result;
    }

    // 6. Patrón numérico formateado: "123-456" o "123 456"
    static const QRegularExpression splitDigitRegex(QStringLiteral("\\b(\\d{3})[-\\s](\\d{3})\\b"));
    auto sMatch = splitDigitRegex.match(combined);
    if (sMatch.hasMatch()) {
        int startPos = sMatch.capturedStart();
        if (startPos > 0 && (combined.at(startPos - 1) == QLatin1Char('+') || combined.at(startPos - 1) == QLatin1Char('/'))) {
            // Número telefónico o fecha
        } else {
            result.detected = true;
            result.code = sMatch.captured(1) + sMatch.captured(2);
            result.originalMatch = sMatch.captured(0);
            result.service = extractService(combined, title, appName);
            return result;
        }
    }

    // 7. Patrón numérico genérico de 4 a 8 dígitos rodeado por límites de palabra
    static const QRegularExpression genericDigitRegex(QStringLiteral("\\b(\\d{4,8})\\b"));
    auto it = genericDigitRegex.globalMatch(combined);
    while (it.hasNext()) {
        auto match = it.next();
        const QString numStr = match.captured(1);
        int start = match.capturedStart();
        int end = match.capturedEnd();

        // Descartar si está pegado a símbolo monetario ($20, 20€)
        if (start > 0 && (combined.at(start - 1) == QLatin1Char('$') || combined.at(start - 1) == QChar(0x20AC))) {
            continue;
        }
        if (end < combined.length() && (combined.at(end) == QChar(0x20AC) || combined.at(end) == QLatin1Char('$'))) {
            continue;
        }

        // Descartar formato horario o de fecha (14:30, 10/11)
        if ((start > 0 && (combined.at(start - 1) == QLatin1Char(':') || combined.at(start - 1) == QLatin1Char('/')))
            || (end < combined.length() && (combined.at(end) == QLatin1Char(':') || combined.at(end) == QLatin1Char('/')))) {
            continue;
        }

        // Descartar años típicos en contexto de fecha si mide 4 dígitos y está entre 1950 y 2035
        if (numStr.length() == 4) {
            int year = numStr.toInt();
            if (year >= 1950 && year <= 2035) {
                const QString beforeSnippet = combined.mid(qMax(0, start - 10), qMin(10, start)).toLower();
                if (beforeSnippet.contains(QLatin1String("año")) || beforeSnippet.contains(QLatin1String("ano"))
                    || beforeSnippet.contains(QLatin1String("del ")) || beforeSnippet.contains(QLatin1String("en "))) {
                    continue;
                }
            }
        }

        result.detected = true;
        result.code = numStr;
        result.originalMatch = match.captured(0);
        result.service = extractService(combined, title, appName);
        return result;
    }

    return result;
}

bool TwoFactorDetector::copyToClipboardSecurely(const QString &code, int autoClearSeconds)
{
    if (code.isEmpty()) {
        return false;
    }

    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        return false;
    }

    auto *mime = new QMimeData();
    mime->setText(code);

    // Espec de privacidad FreeDesktop / KDE: No guardar contraseñas ni OTP en el historial de Klipper
    mime->setData(QStringLiteral("x-kde-passwordManagerHint"), QByteArrayLiteral("secret"));

    clipboard->setMimeData(mime);

    // Temporizador de seguridad: si el usuario no ha copiado otra cosa transcurridos N segundos, limpiar el código
    if (autoClearSeconds > 0) {
        QTimer::singleShot(autoClearSeconds * 1000, [code]() {
            QClipboard *clip = QGuiApplication::clipboard();
            if (clip && clip->text() == code) {
                clip->clear();
            }
        });
    }

    return true;
}
