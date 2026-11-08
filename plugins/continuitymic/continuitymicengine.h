/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QByteArray>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QString>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

class ContinuityMicEngine : public QObject
{
    Q_OBJECT

public:
    explicit ContinuityMicEngine(QObject *parent = nullptr);
    ~ContinuityMicEngine() override;

    bool start(const QString &deviceName = QStringLiteral("Remote Device"));
    void stop();
    bool isRunning() const;

    void pushAudio(const QByteArray &pcmData);

    static void onProcessCallback(void *userdata);
    static void onStateChangedCallback(void *userdata, enum pw_stream_state old, enum pw_stream_state state, const char *error);

Q_SIGNALS:
    void stateChanged(bool running);
    void errorOccurred(const QString &errorMessage);

private:
    void onProcess();

    struct pw_thread_loop *m_threadLoop = nullptr;
    struct pw_context *m_context = nullptr;
    struct pw_core *m_core = nullptr;
    struct pw_stream *m_stream = nullptr;
    struct spa_hook m_streamListener;

    mutable QMutex m_bufferMutex;
    QByteArray m_audioRingBuffer;
    bool m_running = false;
    uint32_t m_rate = 48000;
    uint32_t m_channels = 2;
};
