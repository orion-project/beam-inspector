#include "app/HelpSystem.h"
#include "windows/PlotWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#endif

    QApplication app(argc, argv);
    app.setApplicationName("Cignus");
    app.setOrganizationName("orion-project.org");
    app.setApplicationVersion(HelpSystem::appVersion());
    app.setStyle("Fusion");

    PlotWindow w;
    w.show();

    return app.exec();
}
