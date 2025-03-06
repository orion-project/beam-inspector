#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include "core/OriTemplates.h"

#include <QObject>

enum class UpdateCheckInterval
{
    None,
    Daily,
    Weekly,
    Monthly,
};

class IAppSettingsListener
{
public:
    IAppSettingsListener();
    virtual ~IAppSettingsListener();

    virtual void settingsChanged() {}
};

class AppSettings : public QObject, public Ori::Notifier<IAppSettingsListener>
{
public:
    static AppSettings& instance();

    AppSettings();

    int propChangeWheelSm = 20;
    int propChangeWheelBig = 100;
    int propChangeArrowSm = 20;
    int propChangeArrowBig = 100;
#ifdef WITH_IDS
    bool idsEnabled;
    QString idsSdkDir;
#endif
    QString colorMap;
    bool useConsole = false;
    bool isDevMode = false;
    bool showGoodnessTextOnPlot = false;
    bool showGoodnessRelative = false;
    UpdateCheckInterval updateCheckInterval = UpdateCheckInterval::Weekly;
    bool roundHardConfigFps = true;
    bool roundHardConfigExp = true;
    double overexposedPixelsPercent = 0.1;

    enum ConfigPages {
        cfgDev,
        cfgDbg,
    #ifdef WITH_IDS
        cfgIds,
    #endif
        cfgOpts,
    };

    void load();
    void save();
    bool edit();

    struct ColorMap
    {
        QString name;
        QString file;
        QString descr;
        bool isCurrent;
        bool isExists;
    };
    QList<ColorMap> colorMaps();
    QString currentColorMap();
    void setCurrentColorMap(const QString& fileName);
    void deleteInvalidColorMaps();
    QString selectColorMapFile();
};

#endif // APP_SETTINGS_H
