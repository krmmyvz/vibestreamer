#pragma once
#include <QObject>
#include <QString>

// Prevents the system from going to sleep and the screen from locking/blanking
// while video is actively playing.
//
// libmpv is embedded via the render API (vo=libmpv) into a QOpenGLWidget, so
// mpv does NOT own a native window and its built-in `stop-screensaver` handling
// never fires. The application must therefore inhibit idle/sleep itself.
//
// Backends:
//   Linux  : D-Bus — org.freedesktop.ScreenSaver (screen lock/blank) and
//            org.freedesktop.PowerManagement.Inhibit (system suspend).
//   Windows: SetThreadExecutionState(ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED).
//   macOS  : IOKit IOPMAssertion (prevent display idle sleep).
//
// All calls are best-effort: if a backend is unavailable the inhibitor stays a
// harmless no-op rather than failing playback.
class SleepInhibitor : public QObject {
    Q_OBJECT
public:
    explicit SleepInhibitor(QObject *parent = nullptr);
    ~SleepInhibitor() override;

    // Idempotent — calling with the current state is a no-op, so it is safe to
    // drive directly from playback signals without debouncing.
    void setInhibited(bool inhibit, const QString &reason = QString());
    bool isInhibited() const { return m_inhibited; }

private:
    void acquire(const QString &reason);
    void release();

    bool m_inhibited = false;

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    unsigned int m_screenSaverCookie = 0;
    unsigned int m_powerCookie       = 0;
    bool         m_hasScreenSaver    = false;
    bool         m_hasPower          = false;
#elif defined(Q_OS_MACOS)
    unsigned int m_assertionId = 0; // IOPMAssertionID
#endif
};
