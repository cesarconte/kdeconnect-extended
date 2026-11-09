/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <KRunner/AbstractRunner>
#include <KRunner/Action>
#include <KRunner/QueryMatch>
#include <KRunner/RunnerContext>

class KdeConnectRunner : public KRunner::AbstractRunner
{
    Q_OBJECT

public:
    explicit KdeConnectRunner(QObject *parent, const KPluginMetaData &metaData);
    ~KdeConnectRunner() override = default;

    void match(KRunner::RunnerContext &context) override;
    void run(const KRunner::RunnerContext &context, const KRunner::QueryMatch &match) override;

private:
    void searchRemoteFiles(KRunner::RunnerContext &context, const QString &deviceId, const QString &devName, const QString &searchTerm);
    void searchContacts(KRunner::RunnerContext &context, const QString &deviceId, const QString &devName, const QString &searchTerm);

    QList<KRunner::Action> m_deviceActions;
    QList<KRunner::Action> m_contactActions;
};
