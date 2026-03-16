#include "mainwindow.h"

#include <QApplication>
#include <QIcon>
#include <clocale>

int main(int argc, char *argv[])
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Vibestreamer"));
    app.setOrganizationName(QStringLiteral("Vibestreamer"));
    app.setApplicationVersion(QStringLiteral("1.3.2"));
    app.setWindowIcon(QIcon(QStringLiteral(":/logo_concept1.svg")));

    // libmpv requires LC_NUMERIC="C" — must be set AFTER QApplication
    // because QApplication constructor resets the locale
    std::setlocale(LC_NUMERIC, "C");

    MainWindow w;
    w.show();

    return app.exec();
}
