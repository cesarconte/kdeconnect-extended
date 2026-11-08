/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "continuitymicengine.h"
#include <QDebug>
#include <spa/param/audio/format-utils.h>
#include <spa/pod/builder.h>

static const struct pw_stream_events streamEvents = {.version = PW_VERSION_STREAM_EVENTS,
                                                     .destroy = nullptr,
                                                     .state_changed = ContinuityMicEngine::onStateChangedCallback,
                                                     .control_info = nullptr,
                                                     .io_changed = nullptr,
                                                     .param_changed = nullptr,
                                                     .add_buffer = nullptr,
                                                     .remove_buffer = nullptr,
                                                     .process = ContinuityMicEngine::onProcessCallback,
                                                     .drained = nullptr,
                                                     .command = nullptr,
                                                     .trigger_done = nullptr};

ContinuityMicEngine::ContinuityMicEngine(QObject *parent)
    : QObject(parent)
{
    pw_init(nullptr, nullptr);
}

ContinuityMicEngine::~ContinuityMicEngine()
{
    stop();
    pw_deinit();
}

bool ContinuityMicEngine::isRunning() const
{
    return m_running;
}

void ContinuityMicEngine::onProcessCallback(void *userdata)
{
    auto *self = static_cast<ContinuityMicEngine *>(userdata);
    if (self) {
        self->onProcess();
    }
}

void ContinuityMicEngine::onStateChangedCallback(void *userdata, enum pw_stream_state oldState, enum pw_stream_state newState, const char *error)
{
    Q_UNUSED(oldState)
    auto *self = static_cast<ContinuityMicEngine *>(userdata);
    if (!self) {
        return;
    }

    if (newState == PW_STREAM_STATE_ERROR) {
        const QString errStr = QString::fromUtf8(error ? error : "Unknown PipeWire stream error");
        qWarning() << "ContinuityMicEngine PipeWire stream error:" << errStr;
        Q_EMIT self->errorOccurred(errStr);
    }
}

bool ContinuityMicEngine::start(const QString &deviceName)
{
    if (m_running) {
        return true;
    }

    m_threadLoop = pw_thread_loop_new("KdeConnectContinuityMic", nullptr);
    if (!m_threadLoop) {
        Q_EMIT errorOccurred(QStringLiteral("Failed to create PipeWire thread loop"));
        return false;
    }

    m_context = pw_context_new(pw_thread_loop_get_loop(m_threadLoop), nullptr, 0);
    if (!m_context) {
        pw_thread_loop_destroy(m_threadLoop);
        m_threadLoop = nullptr;
        Q_EMIT errorOccurred(QStringLiteral("Failed to create PipeWire context"));
        return false;
    }

    if (pw_thread_loop_start(m_threadLoop) < 0) {
        pw_context_destroy(m_context);
        pw_thread_loop_destroy(m_threadLoop);
        m_context = nullptr;
        m_threadLoop = nullptr;
        Q_EMIT errorOccurred(QStringLiteral("Failed to start PipeWire thread loop"));
        return false;
    }

    pw_thread_loop_lock(m_threadLoop);

    m_core = pw_context_connect(m_context, nullptr, 0);
    if (!m_core) {
        pw_thread_loop_unlock(m_threadLoop);
        stop();
        Q_EMIT errorOccurred(QStringLiteral("Failed to connect to PipeWire core"));
        return false;
    }

    const QString nodeDesc = QStringLiteral("Continuity Mic (%1)").arg(deviceName);

    struct pw_properties *props = pw_properties_new(PW_KEY_MEDIA_TYPE,
                                                    "Audio",
                                                    PW_KEY_MEDIA_CATEGORY,
                                                    "Capture",
                                                    PW_KEY_MEDIA_ROLE,
                                                    "Communication",
                                                    PW_KEY_NODE_NAME,
                                                    "kdeconnect_continuity_mic",
                                                    PW_KEY_NODE_DESCRIPTION,
                                                    nodeDesc.toUtf8().constData(),
                                                    PW_KEY_APP_NAME,
                                                    "KDE Connect Continuity",
                                                    PW_KEY_APP_ICON_NAME,
                                                    "audio-input-microphone",
                                                    nullptr);

    m_stream = pw_stream_new(m_core, "Continuity Microphone Audio Source", props);
    if (!m_stream) {
        pw_thread_loop_unlock(m_threadLoop);
        stop();
        Q_EMIT errorOccurred(QStringLiteral("Failed to create PipeWire stream"));
        return false;
    }

    pw_stream_add_listener(m_stream, &m_streamListener, &streamEvents, this);

    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    struct spa_audio_info_raw info = {};
    info.format = SPA_AUDIO_FORMAT_S16_LE;
    info.rate = m_rate;
    info.channels = m_channels;

    const struct spa_pod *params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

    int res = pw_stream_connect(m_stream,
                                PW_DIRECTION_OUTPUT,
                                PW_ID_ANY,
                                static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS),
                                params,
                                1);

    pw_thread_loop_unlock(m_threadLoop);

    if (res < 0) {
        stop();
        Q_EMIT errorOccurred(QStringLiteral("Failed to connect PipeWire stream"));
        return false;
    }

    m_running = true;
    Q_EMIT stateChanged(true);
    return true;
}

void ContinuityMicEngine::stop()
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
        QMutexLocker locker(&m_bufferMutex);
        m_audioRingBuffer.clear();
    }

    if (m_running) {
        m_running = false;
        Q_EMIT stateChanged(false);
    }
}

void ContinuityMicEngine::pushAudio(const QByteArray &pcmData)
{
    if (!m_running || pcmData.isEmpty()) {
        return;
    }

    QMutexLocker locker(&m_bufferMutex);
    // Limitar el buffer a un máximo de ~200ms de audio a 48kHz estéreo 16-bit (192000 bytes/s -> ~38400 bytes)
    // para asegurar baja latencia estricta (<30ms) en la cadena PipeWire
    constexpr int MAX_BUFFER_BYTES = 38400;
    if (m_audioRingBuffer.size() > MAX_BUFFER_BYTES) {
        m_audioRingBuffer.remove(0, m_audioRingBuffer.size() - (MAX_BUFFER_BYTES / 2));
    }
    m_audioRingBuffer.append(pcmData);
}

void ContinuityMicEngine::onProcess()
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

    const uint32_t maxBytes = buf->datas[0].maxsize;
    uint32_t filledBytes = 0;

    {
        QMutexLocker locker(&m_bufferMutex);
        if (!m_audioRingBuffer.isEmpty()) {
            filledBytes = std::min(maxBytes, static_cast<uint32_t>(m_audioRingBuffer.size()));
            memcpy(dst, m_audioRingBuffer.constData(), filledBytes);
            m_audioRingBuffer.remove(0, filledBytes);
        }
    }

    // Si no hay datos suficientes de audio, rellenamos con silencio (0)
    // para evitar ruidos de estática o xruns en la cadena DSP de EasyEffects
    if (filledBytes < maxBytes) {
        memset(static_cast<char *>(dst) + filledBytes, 0, maxBytes - filledBytes);
    }

    buf->datas[0].chunk->offset = 0;
    buf->datas[0].chunk->stride = sizeof(int16_t) * m_channels;
    buf->datas[0].chunk->size = maxBytes;

    pw_stream_queue_buffer(m_stream, b);
}
