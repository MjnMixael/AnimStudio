#include "Windows/AnimStudio.h"
#include <QApplication>
#include <QStringList>
#include <QFileInfo>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/Resources/animstudio.ico"));

    QStringList args = app.arguments();

    QString fileToOpen;
    if (args.size() >= 2) {
        fileToOpen = args.at(1);  // First real argument is the file
        if (!QFileInfo::exists(fileToOpen)) {
            fileToOpen.clear();  // Don't try to load nonexistent file
        }
    }

    AnimStudio window;
    window.show();

    if (!fileToOpen.isEmpty()) {
        window.loadFile(fileToOpen);  // Use your real loader here
    }

    return app.exec();
}
