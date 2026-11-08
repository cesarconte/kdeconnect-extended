/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "continuitycameraengine.h"
#include <QDebug>
#include <spa/param/video/format-utils.h>
#include <spa/pod/builder.h>

static const struct pw_stream_events cameraStreamEvents = {.version = PW_VERSION_STREAM_EVENTS,
                                                           .destroy = nullptr,
                                                           .state_changed = ContinuityCameraEngine::onStateChangedCallback,
                                                           .control_info = nullptr,
                                                           .io_changed = nullptr,
                                                           .param_changed = nullptr,
                                                           .add_buffer = nullptr,
                                                           .remove_buffer = nullptr,
                                                           .process = ContinuityCameraEngine::onProcessCallback,
                                                           .drained = nullptr,
                                                           .command = nullptr,
                                                           .trigger_done = nullptr};

ContinuityCameraEngine::ContinuityCameraEngine(QObject *parent)
    : QObject(parent)
{
    pw_init(nullptr, nullptr);
}

ContinuityCameraEngine::~ContinuityCameraEngine()
{
    stop();
    pw_deinit();
}

bool ContinuityCameraEngine::isRunning() const
{
    return m_running;
}

void ContinuityCameraEngine::onProcessCallback(void *userdata)
{
    auto *self = static_cast<ContinuityCameraEngine *>(userdata);
    if (self) {
        self->onProcess();
    }
}

void ContinuityCameraEngine::onStateChangedCallback(void *userdata, enum pw_stream_state oldState, enum pw_stream_state newState, const char *error)
{
    Q_UNUSED(oldState)
    auto *self = static_cast<ContinuityCameraEngine *>(userdata);
    if (!self) {
        return;
    }

    if (newState == PW_STREAM_STATE_ERROR) {
        const QString errStr = QString::fromUtf8(error ? error : "Unknown PipeWire camera stream error");
        qWarning() << "ContinuityCameraEngine PipeWire stream error:" << errStr;
        Q_EMIT self->errorOccurred(errStr);
    }
}

bool ContinuityCameraEngine::start(const QString &deviceName, uint32_t width, uint32_t height)
{
    if (m_running) {
        return true;
    }

    m_width = width;
    m_height = height;

    m_threadLoop = pw_thread_loop_new("KdeConnectContinuityCamera", nullptr);
    if (!m_threadLoop) {
        Q_EMIT errorOccurred(QStringLiteral("Failed to create PipeWire thread loop for camera"));
        return false;
    }

    m_context = pw_context_new(pw_thread_loop_get_loop(m_threadLoop), nullptr, 0);
    if (!m_context) {
        pw_thread_loop_destroy(m_threadLoop);
        m_threadLoop = nullptr;
        Q_EMIT errorOccurred(QStringLiteral("Failed to create PipeWire context for camera"));
        return false;
    }

    if (pw_thread_loop_start(m_threadLoop) < 0) {
        pw_context_destroy(m_context);
        pw_thread_loop_destroy(m_threadLoop);
        m_context = nullptr;
        m_threadLoop = nullptr;
        Q_EMIT errorOccurred(QStringLiteral("Failed to start PipeWire camera thread loop"));
        return false;
    }

    pw_thread_loop_lock(m_threadLoop);

    m_core = pw_context_connect(m_context, nullptr, 0);
    if (!m_core) {
        pw_thread_loop_unlock(m_threadLoop);
        stop();
        Q_EMIT errorOccurred(QStringLiteral("Failed to connect to PipeWire core for camera"));
        return false;
    }

    const QString nodeDesc = QStringLiteral("Continuity Camera (%1)").arg(deviceName);

    struct pw_properties *props = pw_properties_new(PW_KEY_MEDIA_TYPE,
                                                    "Video",
                                                    PW_KEY_MEDIA_CATEGORY,
                                                    "Capture",
                                                    PW_KEY_MEDIA_ROLE,
                                                    "Camera",
                                                    PW_KEY_NODE_NAME,
                                                    "kdeconnect_continuity_camera",
                                                    PW_KEY_NODE_DESCRIPTION,
                                                    nodeDesc.toUtf8().constData(),
                                                    PW_KEY_APP_NAME,
                                                    "KDE Connect Continuity",
                                                    PW_KEY_APP_ICON_NAME,
                                                    "camera-web",
                                                    nullptr);

    m_stream = pw_stream_new(m_core, "Continuity Camera Video Source", props);
    if (!m_stream) {
        pw_thread_loop_unlock(m_threadLoop);
        stop();
        Q_EMIT errorOccurred(QStringLiteral("Failed to create PipeWire camera stream"));
        return false;
    }

    pw_stream_add_listener(m_stream, &m_streamListener, &cameraStreamEvents, this);

    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    struct spa_video_info_raw info = {};
    info.format = SPA_VIDEO_FORMAT_RGBA;
    info.size.width = m_width;
    info.size.height = m_height;
    info.framerate.num = m_fps;
    info.framerate.denom = 1;
    info.max_framerate.num = m_fps;
    info.max_framerate.denom = 1;

    const struct spa_pod *params[1];
    params[0] = spa_format_video_raw_build(&b, SPA_PARAM_EnumFormat, &info);

    int res = pw_stream_connect(m_stream,
                                PW_DIRECTION_OUTPUT,
                                PW_ID_ANY,
                                static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS),
                                params,
                                1);

    pw_thread_loop_unlock(m_threadLoop);

    if (res < 0) {
        stop();
        Q_EMIT errorOccurred(QStringLiteral("Failed to connect PipeWire camera stream"));
        return false;
    }

    m_running = true;
    Q_EMIT stateChanged(true);
    return true;
}

void ContinuityCameraEngine::stop()
{
    if (m_threadLoop) {
        pw_thread_loop_lock(m_threadLoop);
    }

    if (m_stream) {
        pw_stream_destroy(m_stream);
        m_stream = nullptr;
    }

    if (m_core) {
        pw_core_disconnect(m_core);
        m_core = nullptr;
    }

    if (m_threadLoop) {
        pw_thread_loop_unlock(m_threadLoop);
        pw_thread_loop_stop(m_threadLoop);
        if (m_context) {
            pw_context_destroy(m_context);
            m_context = nullptr;
        }
        pw_thread_loop_destroy(m_threadLoop);
        m_threadLoop = nullptr;
    } else if (m_context) {
        pw_context_destroy(m_context);
        m_context = nullptr;
    }

    {
        QMutexLocker locker(&m_frameMutex);
        m_currentFrame = QImage();
    }

    if (m_running) {
        m_running = false;
        Q_EMIT stateChanged(false);
    }
}

void ContinuityCameraEngine::pushFrame(const QImage &frame)
{
    if (!m_running || frame.isNull()) {
        return;
    }

    QImage scaled = frame;
    if (scaled.size() != QSize(m_width, m_height)) {
        scaled = scaled.scaled(m_width, m_height, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    }
    if (scaled.format() != QImage::Format_RGBA8888) {
        scaled = scaled.convertToFormat(QImage::Format_RGBA8888);
    }

    QMutexLocker locker(&m_frameMutex);
    m_currentFrame = scaled;
}

void ContinuityCameraEngine::pushJpegData(const QByteArray &jpegData)
{
    if (!m_running || jpegData.isEmpty()) {
        return;
    }

    QImage img;
    if (img.loadFromData(jpegData, "JPEG") || img.loadFromData(jpegData, "PNG")) {
        pushFrame(img);
    }
}

void ContinuityCameraEngine::onProcess()
{
    struct pw_buffer *b = pw_stream_dequeue_buffer(m_stream);
    if (!b) {
        return;
    }

    struct spa_buffer *buf = b->buffer;
    void *dst = buf->datas[0].data;
    if (!dst) {
        pw_stream_queue_buffer(m_stream, b);
        return;
    }

    const uint32_t stride = m_width * 4;
    const uint32_t expectedBytes = stride * m_height;

    {
        QMutexLocker locker(&m_frameMutex);
        if (!m_currentFrame.isNull() && m_currentFrame.sizeInBytes() >= expectedBytes) {
            memcpy(dst, m_currentFrame.constBits(), expectedBytes);
        } else {
            // Fondo neutro / negro limpio si no hay frame disponible
            memset(dst, 0, expectedBytes);
        }
    }

    buf->datas[0].chunk->offset = 0;
    buf->datas[0].chunk->stride = stride;
    buf->datas[0].chunk->size = expectedBytes;

    pw_stream_queue_buffer(m_stream, b);
}
