#include "app/AppSettings.h"
#include "app/HelpSystem.h"
#include "windows/PlotWindow.h"

#include "helpers/OriTheme.h"
#include "tools/OriDebug.h"
#include "tools/OriHelpWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>

#ifndef Q_OS_WIN
#include <iostream>
#endif

int main(int argc, char *argv[])
{
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#endif

    QApplication app(argc, argv);
    app.setApplicationName("Beam Inspector");
    app.setOrganizationName("orion-project.org");
    app.setApplicationVersion(HelpSystem::appVersion());
    app.setStyle("Fusion");

    QCommandLineParser parser;
    auto optionHelp = parser.addHelpOption();
    auto optionVersion = parser.addVersionOption();
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    parser.setApplicationDescription("Camera based beam profiler");
    QCommandLineOption optionDevMode("dev"); optionDevMode.setFlags(QCommandLineOption::HiddenFromHelp);
    QCommandLineOption optionConsole("console"); optionConsole.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOptions({optionDevMode, optionConsole});

    if (!parser.parse(QApplication::arguments()))
    {
#ifdef Q_OS_WIN
        QMessageBox::critical(nullptr, app.applicationName(), parser.errorText());
#else
        std::cerr << qPrintable(parser.errorText()) << std::endl;
#endif
        return 1;
    }

    // These will quite the app
    if (parser.isSet(optionHelp))
        parser.showHelp();
    if (parser.isSet(optionVersion))
        parser.showVersion();

    // It's only useful on Windows where there is no
    // direct way to use the console for GUI applications.
    if (parser.isSet(optionConsole) || AppSettings::instance().useConsole)
        Ori::Debug::installMessageHandler(AppSettings::instance().saveLogFile);

    // Load application settings before any command start
    AppSettings::instance().isDevMode = parser.isSet(optionDevMode);
    Ori::HelpWindow::isDevMode = parser.isSet(optionDevMode);

    // Call `setStyleSheet` after setting loaded
    // to be able to apply custom colors.
    app.setStyleSheet(Ori::Theme::makeStyleSheet(Ori::Theme::loadRawStyleSheet()));

    PlotWindow w;
    w.show();

    return app.exec();
}
