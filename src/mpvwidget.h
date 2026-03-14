#pragma once
#include <QOpenGLWidget>
#include <QString>
#include <QTimer>

#include <mpv/client.h>
#include <mpv/render_gl.h>

struct TrackInfo {
    int64_t id = 0;
    QString type;     // "video", "audio", "sub"
    QString title;
    QString lang;
    QString codec;
    bool selected = false;
};

class MpvWidget : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit MpvWidget(QWidget *parent = nullptr);
    ~MpvWidget() override;

    void play(const QString &url);
    void stop();
    void setVolume(int v);       // 0–100
    int  volume() const;
    void setPause(bool paused);
    bool isPaused() const;
    void seek(double seconds);
    double duration() const;
    double position() const;

    void setSpeed(double speed);   // 0.5 – 2.0

    // Track selection
    QList<TrackInfo> trackList() const;
    void setAudioTrack(int64_t id);
    void setSubtitleTrack(int64_t id);

    // Media info
    int videoWidth() const;
    int videoHeight() const;
    QString videoCodec() const;
    QString audioCodec() const;

    // Pass extra options before first play
    void setOption(const QString &name, const QString &value);

signals:
    void durationChanged(double secs);
    void positionChanged(double secs);
    void pauseChanged(bool paused);
    void playbackStarted();
    void playbackFinished();
    void errorOccurred(const QString &message);
    void trackListChanged();

protected:
    void initializeGL() override;
    void paintGL()      override;
    void resizeGL(int w, int h) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;

private slots:
    void onMpvEvents();

private:
    static void onMpvUpdate(void *ctx);
    void handleMpvEvent(mpv_event *event);
    void observeProperties();

    mpv_handle         *m_mpv   = nullptr;
    mpv_render_context *m_gl    = nullptr;
    QTimer              m_eventTimer;
    bool                m_glReady = false;
};
