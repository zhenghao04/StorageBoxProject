#include "StorageBoxApp.h"

#include <QApplication>
#include <QIcon>

#ifndef STORAGEBOX_VERSION
#define STORAGEBOX_VERSION "0.0.0"
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("Storage Box Launcher");
    QApplication::setApplicationVersion(STORAGEBOX_VERSION);
    QApplication::setOrganizationName("StorageBoxProject");
    QApplication::setQuitOnLastWindowClosed(false);
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app_icon.png")));

    StorageBoxApp launcher;
    launcher.start();

    return QApplication::exec();
}
