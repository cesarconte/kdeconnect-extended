/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QByteArray>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QString>

#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>

class ContinuityCameraEngine : public QObject
{
    Q_OBJECT

public:
    explicit ContinuityCameraEngine(QObject *parent = nullptr);
    ~ContinuityCameraEngine() override;

    bool start(const QString &deviceName = QStringLiteral("Remote Device"), uint32_t width = 1280, uint32_t height = 720);
    void stop();
    bool isRunning() const;

    void pushFrame(const QImage &frame);
    void pushJpegData(const QByteArray &jpegData);

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

    mutable QMutex m_frameMutex;
    QImage m_currentFrame;
    bool m_running = false;
    uint32_t m_width = 1280;
    uint32_t m_height = 720;
    uint32_t m_fps = 30;
};
