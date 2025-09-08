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

    virtual void settingsChanged(bool affectsCamera) {}
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
    bool saveLogFile = false;
    bool isDevMode = false;
    bool showGoodnessTextOnPlot = false;
    bool showGoodnessRelative = false;
#ifdef SHOW_ALL_PIXEL_FORMATS
    // In camera settings dialog, show all pixel formats provided by camera (see IdsCameraConfig::camFormats)
    // Otherwise, show only formats whose decoding is supported (see IdsCamera::supportedFormats)
    bool showAllPixelFormats = false;
#endif
    UpdateCheckInterval updateCheckInterval = UpdateCheckInterval::Weekly;
    bool roundHardConfigFps = true;
    bool roundHardConfigExp = true;
    double overexposedPixelsPercent = 0.1;
    QChar copyResultsSeparator = ',';
    bool copyResultsJustified = true;
    QMap<QChar, QString> resultsSeparators() const;
    int crosshairRadius = 5;
    int crosshairExtent = 5;
    int crosshairPenWidth = 1;
    int crosshairTextOffsetX = 10;
    int crosshairTextOffsetY = 0;
    int crosshaitTextSize = 14;
    bool tableShowXC = true;
    bool tableShowYC = true;
    bool tableShowDX = true;
    bool tableShowDY = true;
    bool tableShowPhi = true;
    bool tableShowEps = true;

    enum ConfigPages {
        cfgDev,
        cfgDbg,
    #ifdef WITH_IDS
        cfgIds,
    #endif
        cfgOpts,
        cfgTable,
        cfgCrosshair,
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
