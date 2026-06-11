#include "StorageBoxApp.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("Storage Box Launcher");
    QApplication::setOrganizationName("StorageBoxProject");
    QApplication::setQuitOnLastWindowClosed(false);
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app_icon.png")));

    StorageBoxApp launcher;
    launcher.start();

    return QApplication::exec();
}
