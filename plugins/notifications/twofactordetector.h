/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QString>
#include <QStringList>

struct TwoFactorResult {
    bool detected = false;
    QString code;
    QString service;
    QString originalMatch;
};

class TwoFactorDetector
{
public:
    static TwoFactorResult detect(const QString &text, const QString &title = QString(), const QString &appName = QString());
    static bool copyToClipboardSecurely(const QString &code, int autoClearSeconds = 90);

private:
    static QString extractService(const QString &text, const QString &title, const QString &appName);
    static bool isKeywordPresent(const QString &lowerText);
};
