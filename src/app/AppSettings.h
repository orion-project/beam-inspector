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
    bool useConsole = false;
    bool isDevMode = false;
    bool showGoodnessTextOnPlot = false;
    bool showGoodnessRelative = false;
    UpdateCheckInterval updateCheckInterval = UpdateCheckInterval::Weekly;
    bool roundHardConfigFps = true;
    bool roundHardConfigExp = true;
    double overexposedPixelsPercent = 0.1;
    QChar copyResultsSeparator = ',';
    bool copyResultsJustified = true;
    QMap<QChar, QString> resultsSeparators() const;

    enum ConfigPages {
        cfgDev,
        cfgDbg,
    #ifdef WITH_IDS
        cfgIds,
    #endif
        cfgOpts,
        cfgTable,
    };

    void load();
    void save();
    bool edit();

    struct ColorMap
    {
        QString name;
        QString file;
        QString descr;
        bool isExists;
    };
    QList<ColorMap> colorMaps();
    static QString colorMapPath(const QString &colorMap);
    static QString defaultColorMap();
    void deleteInvalidColorMaps();
    QString selectColorMapFile();
};

#endif // APP_SETTINGS_H
