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

        KRunner::Action flickAction(QStringLiteral("flick"), QStringLiteral("arrow-right-double"), QStringLiteral("Lanzar archivo al móvil (Spatial Flick)"));
        QCOMPARE(flickAction.id(), QStringLiteral("flick"));
        QCOMPARE(flickAction.iconSource(), QStringLiteral("arrow-right-double"));

        QList<KRunner::Action> actions = {ringAction, micAction, flickAction};
        QCOMPARE(actions.count(), 3);
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

    void testRemoteFileMatchConstruction()
    {
        KRunner::QueryMatch match;
        const QString remotePath = QStringLiteral("/run/user/1000/f151e55ead63488c839b638f43e6d1ce/DCIM/Camera/IMG_20261109_120000.jpg");
        match.setId(QStringLiteral("kdeconnect:file:") + remotePath);
        match.setData(remotePath);
        match.setText(QStringLiteral("IMG_20261109_120000.jpg"));
        match.setSubtext(QStringLiteral("En Redmi Note 14 Pro 5G · Camera"));
        match.setIconName(QStringLiteral("image-jpeg"));
        match.setRelevance(0.80);

        QVERIFY(match.data().toString().startsWith(QLatin1Char('/')));
        QCOMPARE(match.data().toString(), remotePath);
        QCOMPARE(match.text(), QStringLiteral("IMG_20261109_120000.jpg"));
        QCOMPARE(match.subtext(), QStringLiteral("En Redmi Note 14 Pro 5G · Camera"));
        QCOMPARE(match.iconName(), QStringLiteral("image-jpeg"));
        QCOMPARE(match.relevance(), 0.80);
    }
};

QTEST_MAIN(KRunnerMatchTest)

#include "krunnermatchtest.moc"
