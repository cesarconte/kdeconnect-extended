/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include <KRunner/Action>
#include <KRunner/QueryMatch>
#include <QTest>

class KRunnerMatchTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testActionConstructors()
    {
        KRunner::Action ringAction(QStringLiteral("ring"), QStringLiteral("irc-voice"), QStringLiteral("Hacer sonar teléfono"));
        QCOMPARE(ringAction.id(), QStringLiteral("ring"));
        QCOMPARE(ringAction.iconSource(), QStringLiteral("irc-voice"));
        QCOMPARE(ringAction.text(), QStringLiteral("Hacer sonar teléfono"));

        KRunner::Action micAction(QStringLiteral("mic"), QStringLiteral("audio-input-microphone"), QStringLiteral("Iniciar Micrófono de Continuidad"));
        QCOMPARE(micAction.id(), QStringLiteral("mic"));
        QCOMPARE(micAction.iconSource(), QStringLiteral("audio-input-microphone"));

        QList<KRunner::Action> actions = {ringAction, micAction};
        QCOMPARE(actions.count(), 2);
    }

    void testQueryMatchConstruction()
    {
        KRunner::QueryMatch match;
        match.setData(QStringLiteral("device-uuid-12345"));
        match.setText(QStringLiteral("Pixel 8 Pro"));
        match.setSubtext(QStringLiteral("Dispositivo KDE Connect conectado"));
        match.setIconName(QStringLiteral("smartphone"));
        match.setRelevance(0.95);

        QCOMPARE(match.data().toString(), QStringLiteral("device-uuid-12345"));
        QCOMPARE(match.text(), QStringLiteral("Pixel 8 Pro"));
        QCOMPARE(match.subtext(), QStringLiteral("Dispositivo KDE Connect conectado"));
        QCOMPARE(match.iconName(), QStringLiteral("smartphone"));
        QCOMPARE(match.relevance(), 0.95);
    }
};

QTEST_MAIN(KRunnerMatchTest)

#include "krunnermatchtest.moc"
