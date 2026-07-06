#include "sleepinhibitor.h"

#include <QCoreApplication>

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#elif defined(Q_OS_WIN)
#include <windows.h>
#elif defined(Q_OS_MACOS)
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

SleepInhibitor::SleepInhibitor(QObject *parent) : QObject(parent) {}

SleepInhibitor::~SleepInhibitor()
{
    if (m_inhibited)
        release();
}

void SleepInhibitor::setInhibited(bool inhibit, const QString &reason)
{
    if (inhibit == m_inhibited)
        return;
    m_inhibited = inhibit;
    if (inhibit)
        acquire(reason.isEmpty() ? QStringLiteral("Playing video") : reason);
    else
        release();
}

// ─── Linux (D-Bus) ───────────────────────────────────────────────────────────
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)

namespace {
// The freedesktop ScreenSaver interface (implemented by KDE, GNOME, XFCE, …)
// blocks screen locking and blanking.
QDBusInterface screenSaverIface(const QDBusConnection &bus)
{
    return QDBusInterface(QStringLiteral("org.freedesktop.ScreenSaver"),
                          QStringLiteral("/org/freedesktop/ScreenSaver"),
                          QStringLiteral("org.freedesktop.ScreenSaver"),
                          bus);
}
// The PowerManagement inhibit interface blocks automatic system suspend.
QDBusInterface powerIface(const QDBusConnection &bus)
{
    return QDBusInterface(QStringLiteral("org.freedesktop.PowerManagement.Inhibit"),
                          QStringLiteral("/org/freedesktop/PowerManagement/Inhibit"),
                          QStringLiteral("org.freedesktop.PowerManagement.Inhibit"),
                          bus);
}
} // namespace

void SleepInhibitor::acquire(const QString &reason)
{
    const QString app = QCoreApplication::applicationName().isEmpty()
                            ? QStringLiteral("Vibestreamer")
                            : QCoreApplication::applicationName();
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return;

    QDBusInterface ss = screenSaverIface(bus);
    QDBusReply<unsigned int> ssReply = ss.call(QStringLiteral("Inhibit"), app, reason);
    if (ssReply.isValid()) {
        m_screenSaverCookie = ssReply.value();
        m_hasScreenSaver = true;
    }

    QDBusInterface pm = powerIface(bus);
    QDBusReply<unsigned int> pmReply = pm.call(QStringLiteral("Inhibit"), app, reason);
    if (pmReply.isValid()) {
        m_powerCookie = pmReply.value();
        m_hasPower = true;
    }
}

void SleepInhibitor::release()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        m_hasScreenSaver = m_hasPower = false;
        return;
    }
    if (m_hasScreenSaver) {
        screenSaverIface(bus).call(QStringLiteral("UnInhibit"), m_screenSaverCookie);
        m_hasScreenSaver = false;
        m_screenSaverCookie = 0;
    }
    if (m_hasPower) {
        powerIface(bus).call(QStringLiteral("UnInhibit"), m_powerCookie);
        m_hasPower = false;
        m_powerCookie = 0;
    }
}

// ─── Windows ─────────────────────────────────────────────────────────────────
#elif defined(Q_OS_WIN)

void SleepInhibitor::acquire(const QString & /*reason*/)
{
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
}

void SleepInhibitor::release()
{
    SetThreadExecutionState(ES_CONTINUOUS);
}

// ─── macOS ───────────────────────────────────────────────────────────────────
#elif defined(Q_OS_MACOS)

void SleepInhibitor::acquire(const QString &reason)
{
    CFStringRef why = CFStringCreateWithCString(kCFAllocatorDefault,
                                                reason.toUtf8().constData(),
                                                kCFStringEncodingUTF8);
    IOPMAssertionID id = 0;
    IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleDisplaySleep,
                                kIOPMAssertionLevelOn,
                                why ? why : CFSTR("Playing video"),
                                &id);
    if (why)
        CFRelease(why);
    m_assertionId = id;
}

void SleepInhibitor::release()
{
    if (m_assertionId) {
        IOPMAssertionRelease(m_assertionId);
        m_assertionId = 0;
    }
}

// ─── Unsupported platform: no-op ─────────────────────────────────────────────
#else

void SleepInhibitor::acquire(const QString &) {}
void SleepInhibitor::release() {}

#endif
