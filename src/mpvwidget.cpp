
#include "mpvwidget.h"

#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <stdexcept>

// ─── OpenGL proc-address helper ──────────────────────────────────────────────

static void *getProcAddress(void * /*ctx*/, const char *name)
{
    const QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (!ctx) return nullptr;
    return reinterpret_cast<void *>(ctx->getProcAddress(name));
}

// ─── MpvWidget ───────────────────────────────────────────────────────────────

MpvWidget::MpvWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    m_mpv = mpv_create();
    if (!m_mpv)
        throw std::runtime_error("Failed to create mpv context");

    // Basic options
    mpv_set_option_string(m_mpv, "terminal",    "no");   // disable terminal spam → reduces I/O CPU
    mpv_set_option_string(m_mpv, "vo",           "libmpv");
    mpv_set_option_string(m_mpv, "hwdec",        "auto"); // enable hardware decoding
    mpv_set_option_string(m_mpv, "cache",        "yes");
    mpv_set_option_string(m_mpv, "cache-secs",   "30");

    if (mpv_initialize(m_mpv) < 0)
        throw std::runtime_error("Failed to initialize mpv");

    mpv_set_wakeup_callback(m_mpv, MpvWidget::onMpvWakeup, this);

    observeProperties();

    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(320, 180);
}

MpvWidget::~MpvWidget()
{
    makeCurrent();
    if (m_gl) {
        mpv_render_context_free(m_gl);
        m_gl = nullptr;
    }
    doneCurrent();

    if (m_mpv) {
        mpv_set_wakeup_callback(m_mpv, nullptr, nullptr);
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
    }
}

// ─── GL lifecycle ────────────────────────────────────────────────────────────

void MpvWidget::initializeGL()
{
    mpv_opengl_init_params glParams{};
    glParams.get_proc_address = getProcAddress;

    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE,        const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glParams},
        {MPV_RENDER_PARAM_INVALID,          nullptr}
    };

    if (mpv_render_context_create(&m_gl, m_mpv, params) < 0)
        throw std::runtime_error("Failed to create mpv GL render context");

    mpv_render_context_set_update_callback(m_gl, MpvWidget::onMpvUpdate, this);
    m_glReady = true;
}

void MpvWidget::paintGL()
{
    if (!m_glReady || !m_gl) return;

    // Drain any pending OpenGL errors left by Qt so libmpv's glGetError() check
    // doesn't see stale errors. Use a bounded loop to avoid an infinite spin
    // if the driver keeps returning errors.
    if (auto *ctx = QOpenGLContext::currentContext()) {
        auto *funcs = ctx->functions();
        for (int i = 0; i < 8 && funcs->glGetError() != 0; ++i);
    }

    const qreal dpr = devicePixelRatio();
    mpv_opengl_fbo fbo{};
    fbo.fbo    = defaultFramebufferObject();
    fbo.w      = static_cast<int>(width()  * dpr);
    fbo.h      = static_cast<int>(height() * dpr);
    fbo.internal_format = 0;

    int flipY = 1;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO,  &fbo},
        {MPV_RENDER_PARAM_FLIP_Y,      &flipY},
        {MPV_RENDER_PARAM_INVALID,     nullptr}
    };

    mpv_render_context_render(m_gl, params);
}

void MpvWidget::resizeGL(int /*w*/, int /*h*/)
{
    update();
}

// ─── mpv update callback (called from mpv's render thread) ───────────────────

void MpvWidget::onMpvUpdate(void *ctx)
{
    auto *self = static_cast<MpvWidget *>(ctx);
    if (!self || !self->m_glReady || !self->m_gl)
        return;

    if (self->m_renderQueued.exchange(true))
        return;

    QMetaObject::invokeMethod(self, [self]() {
        self->m_renderQueued = false;
        if (self->m_glReady && self->m_gl)
            self->update();
    }, Qt::QueuedConnection);
}

void MpvWidget::onMpvWakeup(void *ctx)
{
    auto *self = static_cast<MpvWidget *>(ctx);
    if (!self)
        return;

    if (self->m_eventsQueued.exchange(true))
        return;

    QMetaObject::invokeMethod(self, [self]() {
        self->m_eventsQueued = false;
        self->onMpvEvents();
    }, Qt::QueuedConnection);
}

// ─── Event handling ──────────────────────────────────────────────────────────

void MpvWidget::observeProperties()
{
    mpv_observe_property(m_mpv, 0, "duration",     MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "time-pos",     MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "pause",        MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "core-idle",    MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "track-list/count", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "volume",       MPV_FORMAT_DOUBLE);
}

void MpvWidget::onMpvEvents()
{
    while (m_mpv) {
        mpv_event *event = mpv_wait_event(m_mpv, 0);
        if (event->event_id == MPV_EVENT_NONE)
            break;
        handleMpvEvent(event);
    }
}

void MpvWidget::handleMpvEvent(mpv_event *event)
{
    switch (event->event_id) {
    case MPV_EVENT_START_FILE:
        emit playbackStarted();
        break;
    case MPV_EVENT_END_FILE: {
        const auto *ef = static_cast<mpv_event_end_file *>(event->data);
        if (ef->reason == MPV_END_FILE_REASON_ERROR)
            emit errorOccurred(QString::fromUtf8(mpv_error_string(ef->error)));
        else
            emit playbackFinished();
        break;
    }
    case MPV_EVENT_PROPERTY_CHANGE: {
        const auto *prop = static_cast<mpv_event_property *>(event->data);
        if (prop->format == MPV_FORMAT_DOUBLE) {
            const double val = *static_cast<double *>(prop->data);
            if (QLatin1String(prop->name) == QLatin1String("duration")) {
                emit durationChanged(val);
            } else if (QLatin1String(prop->name) == QLatin1String("time-pos")) {
                emit positionChanged(val);
            }
        } else if (prop->format == MPV_FORMAT_FLAG) {
            const int val = *static_cast<int *>(prop->data);
            if (QLatin1String(prop->name) == QLatin1String("pause")) {
                emit pauseChanged(val != 0);
            }
        } else if (prop->format == MPV_FORMAT_INT64) {
            if (QLatin1String(prop->name) == QLatin1String("track-list/count"))
                emit trackListChanged();
        }
        break;
    }
    default:
        break;
    }
}

// ─── Playback controls ───────────────────────────────────────────────────────

void MpvWidget::play(const QString &url)
{
    const QByteArray u = url.toUtf8();
    const char *args[] = {"loadfile", u.constData(), nullptr};
    mpv_command_async(m_mpv, 0, args);
}

void MpvWidget::stop()
{
    const char *args[] = {"stop", nullptr};
    mpv_command_async(m_mpv, 0, args);
}

void MpvWidget::setVolume(int v)
{
    double dv = static_cast<double>(qBound(0, v, 200));
    mpv_set_property_async(m_mpv, 0, "volume", MPV_FORMAT_DOUBLE, &dv);
}

int MpvWidget::volume() const
{
    double v = 100.0;
    mpv_get_property(m_mpv, "volume", MPV_FORMAT_DOUBLE, &v);
    return static_cast<int>(v);
}

void MpvWidget::setPause(bool paused)
{
    int v = paused ? 1 : 0;
    mpv_set_property_async(m_mpv, 0, "pause", MPV_FORMAT_FLAG, &v);
}

bool MpvWidget::isPaused() const
{
    int v = 0;
    mpv_get_property(m_mpv, "pause", MPV_FORMAT_FLAG, &v);
    return v != 0;
}

void MpvWidget::seek(double seconds)
{
    const QByteArray s = QByteArray::number(seconds, 'f', 3);
    const char *args[] = {"seek", s.constData(), "absolute", nullptr};
    mpv_command_async(m_mpv, 0, args);
}

double MpvWidget::duration() const
{
    double d = 0.0;
    mpv_get_property(m_mpv, "duration", MPV_FORMAT_DOUBLE, &d);
    return d;
}

double MpvWidget::position() const
{
    double p = 0.0;
    mpv_get_property(m_mpv, "time-pos", MPV_FORMAT_DOUBLE, &p);
    return p;
}

void MpvWidget::setSpeed(double speed)
{
    double s = qBound(0.1, speed, 10.0);
    mpv_set_property_async(m_mpv, 0, "speed", MPV_FORMAT_DOUBLE, &s);
}

void MpvWidget::setOption(const QString &name, const QString &value)
{
    mpv_set_option_string(m_mpv, name.toUtf8().constData(),
                          value.toUtf8().constData());
}

void MpvWidget::setMpvProperty(const QString &name, const QString &value)
{
    // Avoid mpv_set_property_async since its string lifetimes are tricky, 
    // and mpv_command_async might be triggering weird internal state.
    // Given these properties are applied BEFORE play(), synchronous call is completely safe.
    mpv_set_property_string(m_mpv, name.toUtf8().constData(), value.toUtf8().constData());
}

// ─── Track list & media info ─────────────────────────────────────────────────

QList<TrackInfo> MpvWidget::trackList() const
{
    QList<TrackInfo> list;
    int64_t count = 0;
    mpv_get_property(m_mpv, "track-list/count", MPV_FORMAT_INT64, &count);

    for (int64_t i = 0; i < count; ++i) {
        TrackInfo t;
        auto strProp = [&](const char *key) -> QString {
            char *val = nullptr;
            const QByteArray p = QStringLiteral("track-list/%1/%2").arg(i).arg(QLatin1String(key)).toUtf8();
            if (mpv_get_property(m_mpv, p.constData(), MPV_FORMAT_STRING, &val) >= 0 && val) {
                QString s = QString::fromUtf8(val);
                mpv_free(val);
                return s;
            }
            return {};
        };
        auto int64Prop = [&](const char *key) -> int64_t {
            int64_t val = 0;
            const QByteArray p = QStringLiteral("track-list/%1/%2").arg(i).arg(QLatin1String(key)).toUtf8();
            mpv_get_property(m_mpv, p.constData(), MPV_FORMAT_INT64, &val);
            return val;
        };
        auto flagProp = [&](const char *key) -> bool {
            int val = 0;
            const QByteArray p = QStringLiteral("track-list/%1/%2").arg(i).arg(QLatin1String(key)).toUtf8();
            mpv_get_property(m_mpv, p.constData(), MPV_FORMAT_FLAG, &val);
            return val != 0;
        };

        t.id       = int64Prop("id");
        t.type     = strProp("type");
        t.title    = strProp("title");
        t.lang     = strProp("lang");
        t.codec    = strProp("codec");
        t.selected = flagProp("selected");
        list.append(t);
    }
    return list;
}

void MpvWidget::setAudioTrack(int64_t id)
{
    mpv_set_property_async(m_mpv, 0, "aid", MPV_FORMAT_INT64, &id);
}

void MpvWidget::setSubtitleTrack(int64_t id)
{
    if (id <= 0) {
        // Disable subtitles
        mpv_set_property_string(m_mpv, "sid", "no");
    } else {
        mpv_set_property_async(m_mpv, 0, "sid", MPV_FORMAT_INT64, &id);
    }
}

int MpvWidget::videoWidth() const
{
    int64_t w = 0;
    mpv_get_property(m_mpv, "video-params/w", MPV_FORMAT_INT64, &w);
    return static_cast<int>(w);
}

int MpvWidget::videoHeight() const
{
    int64_t h = 0;
    mpv_get_property(m_mpv, "video-params/h", MPV_FORMAT_INT64, &h);
    return static_cast<int>(h);
}

QString MpvWidget::videoCodec() const
{
    char *val = nullptr;
    if (mpv_get_property(m_mpv, "video-codec", MPV_FORMAT_STRING, &val) >= 0 && val) {
        QString s = QString::fromUtf8(val);
        mpv_free(val);
        return s;
    }
    return {};
}

QString MpvWidget::audioCodec() const
{
    char *val = nullptr;
    if (mpv_get_property(m_mpv, "audio-codec-name", MPV_FORMAT_STRING, &val) >= 0 && val) {
        QString s = QString::fromUtf8(val);
        mpv_free(val);
        return s;
    }
    return {};
}

void MpvWidget::mouseDoubleClickEvent(QMouseEvent *e)
{
    QOpenGLWidget::mouseDoubleClickEvent(e);
    // Delegate fullscreen toggle to parent window via signal/slot wiring in MainWindow
    QMetaObject::invokeMethod(window(), "toggleFullScreen", Qt::QueuedConnection);
}
