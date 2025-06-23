#include "AppSettings.h"

#include "app/HelpSystem.h"

#include "dialogs/OriConfigDlg.h"
#include "helpers/OriDialogs.h"
#include "tools/OriSettings.h"

#include <QApplication>
#include <QDir>
#include <QFileDialog>

using namespace Ori::Dlg;

#define GROUP_PLOT "Plot"
#define KEY_COLOR_MAPS "colorMaps"
#define KEY_COLOR_MAPS_DIR "colorMapsDir"

//------------------------------------------------------------------------------
//                            IAppSettingsListener
//------------------------------------------------------------------------------

IAppSettingsListener::IAppSettingsListener()
{
    AppSettings::instance().registerListener(this);
}

IAppSettingsListener::~IAppSettingsListener()
{
    AppSettings::instance().unregisterListener(this);
}

//------------------------------------------------------------------------------
//                                 AppSettings
//------------------------------------------------------------------------------

Q_GLOBAL_STATIC(AppSettings, __instance);

#define LOAD(option, type, default_value)\
    option = s.settings()->value(QStringLiteral(#option), default_value).to ## type()

#define SAVE(option)\
    s.settings()->setValue(QStringLiteral(#option), option)

AppSettings& AppSettings::instance()
{
    return *__instance;
}

AppSettings::AppSettings() : QObject()
{
    load();
}

void AppSettings::load()
{
    Ori::Settings s;

    s.beginGroup("Debug");
    LOAD(useConsole, Bool, false);
    LOAD(saveLogFile, Bool, false);
    LOAD(showGoodnessTextOnPlot, Bool, false);
    LOAD(showGoodnessRelative, Bool, false);

#ifdef WITH_IDS
    s.beginGroup("IdsCamera");
    LOAD(idsEnabled, Bool, true);
    LOAD(idsSdkDir, String, "C:/Program Files/IDS/ids_peak/comfort_sdk/api/lib/x86_64/");
#endif

    s.beginGroup("DeviceControl");
    LOAD(propChangeWheelSm, Int, 20);
    LOAD(propChangeWheelBig, Int, 100);
    LOAD(propChangeArrowSm, Int, 20);
    LOAD(propChangeArrowBig, Int, 100);

    s.beginGroup("Update");
    updateCheckInterval = UpdateCheckInterval(s.value("checkInterval", int(UpdateCheckInterval::Weekly)).toInt());

    s.beginGroup("Tweaks");
    LOAD(roundHardConfigFps, Bool, true);
    LOAD(roundHardConfigExp, Bool, true);
    LOAD(overexposedPixelsPercent, Double, 0.1);

    s.beginGroup("Table");
    LOAD(copyResultsSeparator, Char, ',');
    LOAD(copyResultsJustified, Bool, true);
    LOAD(tableShowXC, Bool, true);
    LOAD(tableShowYC, Bool, true);
    LOAD(tableShowDX, Bool, true);
    LOAD(tableShowDY, Bool, true);
    LOAD(tableShowPhi, Bool, true);
    LOAD(tableShowEps, Bool, true);
    
    s.beginGroup("Crosshair");
    LOAD(crosshairRadius, Int, 5);
    LOAD(crosshairExtent, Int, 5);
    LOAD(crosshairPenWidth, Int, 1);
    LOAD(crosshairTextOffsetX, Int, 10);
    LOAD(crosshairTextOffsetY, Int, 0);
    LOAD(crosshaitTextSize, Int, 14);
}

void AppSettings::save()
{
    Ori::Settings s;

    s.beginGroup("Debug");
    SAVE(useConsole);
    SAVE(saveLogFile);
    SAVE(showGoodnessTextOnPlot);
    SAVE(showGoodnessRelative);

#ifdef WITH_IDS
    s.beginGroup("IdsCamera");
    SAVE(idsEnabled);
    SAVE(idsSdkDir);
#endif

    s.beginGroup("DeviceControl");
    SAVE(propChangeWheelSm);
    SAVE(propChangeWheelBig);
    SAVE(propChangeArrowSm);
    SAVE(propChangeArrowBig);

    s.beginGroup("Update");
    s.setValue("checkInterval", int(updateCheckInterval));

    s.beginGroup("Tweaks");
    SAVE(roundHardConfigFps);
    SAVE(roundHardConfigExp);
    SAVE(overexposedPixelsPercent);

    s.beginGroup("Table");
    SAVE(copyResultsSeparator);
    SAVE(copyResultsJustified);
    SAVE(tableShowXC);
    SAVE(tableShowYC);
    SAVE(tableShowDX);
    SAVE(tableShowDY);
    SAVE(tableShowPhi);
    SAVE(tableShowEps);

    s.beginGroup("Crosshair");
    SAVE(crosshairRadius);
    SAVE(crosshairExtent);
    SAVE(crosshairPenWidth);
    SAVE(crosshairTextOffsetX);
    SAVE(crosshairTextOffsetY);
    SAVE(crosshaitTextSize);
}

QMap<QChar, QString> AppSettings::resultsSeparators() const
{
    return {
        { ' ', tr("Space") },
        { '\t', tr("Tab") },
        { ',', tr(",") },
        { ';', tr(";") }
    };
}

bool AppSettings::edit()
{
    ConfigDlgOpts opts;
    opts.objectName = "AppSettingsDlg";
    opts.pageIconSize = 32;
    opts.pages = {
        ConfigPage(cfgTable, tr("Table"), ":/toolbar/table")
            .withHelpTopic("app_settings_table"),
        ConfigPage(cfgDev, tr("Device Control"), ":/toolbar/hardware")
            .withHelpTopic("app_settings_hard"),
    #ifdef WITH_IDS
        ConfigPage(cfgIds, tr("IDS Camera"), ":/toolbar/camera")
            .withHelpTopic("app_settings_ids"),
    #endif
        ConfigPage(cfgDbg, tr("Debug"), ":/toolbar/bug"),
        ConfigPage(cfgOpts, tr("Options"), ":/toolbar/options")
            .withHelpTopic("app_settings_opts"),
        ConfigPage(cfgCrosshair, tr("Crosshairs"), ":/toolbar/crosshair")
    };
    opts.onHelpRequested = [](const QString &topic){ HelpSystem::topic(topic); };
    auto seps = resultsSeparators();
    int sepIdx = seps.keys().indexOf(copyResultsSeparator);
    bool old_tableShowXC = tableShowXC;
    bool old_tableShowYC = tableShowYC;
    bool old_tableShowDX = tableShowDX;
    bool old_tableShowDY = tableShowDY;
    bool old_tableShowPhi = tableShowPhi;
    bool old_tableShowEps = tableShowEps;
    opts.items = {
        new ConfigItemSection(cfgTable, tr("Copy results")),
        new ConfigItemRadio(cfgTable, tr("Value separator"), seps.values(), &sepIdx),
        new ConfigItemBool(cfgTable, tr("Justify values in columns"), &copyResultsJustified),
        
        new ConfigItemSpace(cfgTable, 12),
        new ConfigItemSection(cfgTable, tr("Show results")),
        new ConfigItemBool(cfgTable, tr("Center X"), &tableShowXC),
        new ConfigItemBool(cfgTable, tr("Center Y"), &tableShowYC),
        new ConfigItemBool(cfgTable, tr("Width X"), &tableShowDX),
        new ConfigItemBool(cfgTable, tr("Width Y"), &tableShowDY),
        new ConfigItemBool(cfgTable, tr("Azimuth"), &tableShowPhi),
        new ConfigItemBool(cfgTable, tr("Ellipticity"), &tableShowEps),

        new ConfigItemSection(cfgDev, tr("Input Fields")),
        (new ConfigItemInt(cfgDev, tr("Small change by mouse wheel, %"), &propChangeWheelSm))
            ->withMinMax(1, 1000),
        (new ConfigItemInt(cfgDev, tr("Big change by mouse wheel, %"), &propChangeWheelBig))
            ->withMinMax(1, 1000)
            ->withHint(tr("Hold Control key for big change")),
        (new ConfigItemInt(cfgDev, tr("Small change by arrow keys, %"), &propChangeArrowSm))
            ->withMinMax(1, 1000),
        (new ConfigItemInt(cfgDev, tr("Big change by arrow keys, %"), &propChangeArrowBig))
            ->withMinMax(1, 1000)
            ->withHint(tr("Hold Control key for big change")),

    #ifdef WITH_IDS
        new ConfigItemBool(cfgIds, tr("Enable"), &idsEnabled),
        new ConfigItemDir(cfgIds, tr("Peak comfortC directory (x64)"), &idsSdkDir),
    #endif

        new ConfigItemBool(cfgDbg, tr("Show log window (restart required)"), &useConsole),
        (new ConfigItemBool(cfgDbg, tr("Save log into file"), &saveLogFile))
            ->withParent(&useConsole),
        new ConfigItemSpace(cfgDbg, 12),
        (new ConfigItemSection(cfgDbg, tr("Multi-pass cell alignment")))
            ->withHint(tr("Toggle multi-roi mode to apply")),
        new ConfigItemBool(cfgDbg, tr("Show goodness values on plot"), &showGoodnessTextOnPlot),
        (new ConfigItemBool(cfgDbg, tr("Show goodness in relative units"), &showGoodnessRelative))
            ->withParent(&showGoodnessTextOnPlot),

        (new ConfigItemDropDown(cfgOpts, tr("Automatically check for updates"), (int*)&updateCheckInterval))
            ->withOption(int(UpdateCheckInterval::None), tr("None"))
            ->withOption(int(UpdateCheckInterval::Daily), tr("Daily"))
            ->withOption(int(UpdateCheckInterval::Weekly), tr("Weekly"))
            ->withOption(int(UpdateCheckInterval::Monthly), tr("Monthly")),

        new ConfigItemSpace(cfgOpts, 12),
        new ConfigItemSection(cfgOpts, tr("Overexposure")),
        new ConfigItemReal(cfgOpts, tr("Show warning when number\nof hot pixels is more than (%)"), &overexposedPixelsPercent),

        new ConfigItemSpace(cfgOpts, 12),
        new ConfigItemSection(cfgOpts, tr("Tweaks")),
        new ConfigItemBool(cfgOpts, tr("Camera control: Round frame rate"), &roundHardConfigFps),
        new ConfigItemBool(cfgOpts, tr("Camera control: Round exposure"), &roundHardConfigExp),
        
        (new ConfigItemInt(cfgCrosshair, tr("Radius"), &crosshairRadius))->withMinMax(0, 20),
        (new ConfigItemInt(cfgCrosshair, tr("Extent"), &crosshairExtent))->withMinMax(0, 20),
        (new ConfigItemInt(cfgCrosshair, tr("Pen width"), &crosshairPenWidth))->withMinMax(0, 5),
        (new ConfigItemInt(cfgCrosshair, tr("Horizontal label offset"), &crosshairTextOffsetX))->withMinMax(-50, 50),
        (new ConfigItemInt(cfgCrosshair, tr("Vertical label offset"), &crosshairTextOffsetY))->withMinMax(-50, 50),
        (new ConfigItemInt(cfgCrosshair, tr("Font size"), &crosshaitTextSize))->withMinMax(10, 32),
    };
    if (ConfigDlg::edit(opts))
    {
        copyResultsSeparator = seps.keys().at(sepIdx);
        save();
        bool affectsCamera =
            old_tableShowXC != tableShowXC ||
            old_tableShowYC != tableShowYC ||
            old_tableShowDX != tableShowDX ||
            old_tableShowDY != tableShowDY ||
            old_tableShowPhi != tableShowPhi ||
            old_tableShowEps != tableShowEps;
        notify(&IAppSettingsListener::settingsChanged, affectsCamera);
        return true;
    }
    return false;
}

static QString colorMapDir()
{
    return qApp->applicationDirPath() + QStringLiteral("/colormaps");
}

QList<AppSettings::ColorMap> AppSettings::colorMaps()
{
    QList<AppSettings::ColorMap> result;
    QDir dir(colorMapDir());
    dir.setSorting(QDir::Name);
    dir.setNameFilters({QStringLiteral("*.csv")});
    auto entries = dir.entryInfoList();
    for (const auto& fi : std::as_const(entries)) {
        QString path = fi.absoluteFilePath();
        ColorMap map {
            .name = fi.baseName(),
            .file = fi.baseName(),
            .isExists = true,
        };
        QString descrFile = path.replace(QStringLiteral(".csv"), QStringLiteral(".ini"));
        if (QFileInfo::exists(descrFile)) {
            QSettings ini(descrFile, QSettings::IniFormat);
            ini.beginGroup("ColorMap");
            map.descr = ini.value("descr").toString();
        } else {
            continue;
        }
        result << map;
    }
    result << ColorMap();
    Ori::Settings s;
    s.beginGroup(GROUP_PLOT);
    auto keys = s.value(KEY_COLOR_MAPS).toStringList();
    for (const auto& file : std::as_const(keys)) {
        QFileInfo fi(file);
        QString path = fi.absoluteFilePath();
        result << ColorMap {
            .name = fi.baseName(),
            .file = path,
            .descr = path,
            .isExists = fi.exists(),
        };
    }
    return result;
}

QString AppSettings::colorMapPath(const QString &colorMap)
{
    QFileInfo fi(colorMap);
    if (!fi.isAbsolute())
        fi = QFileInfo(colorMapDir() + '/' + colorMap);
    if (fi.suffix().isEmpty())
        fi = QFileInfo(fi.absoluteFilePath() + QStringLiteral(".csv"));
    return fi.absoluteFilePath();
}

QString AppSettings::defaultColorMap()
{
    return QStringLiteral("CET-R2");
}

void AppSettings::deleteInvalidColorMaps()
{
    Ori::Settings s;
    s.beginGroup(GROUP_PLOT);
    auto colorMaps = s.value(KEY_COLOR_MAPS).toStringList();
    int count = 0;
    for (int i = colorMaps.size()-1; i >= 0; i--)
        if (!QFileInfo::exists(colorMaps.at(i))) {
            colorMaps.removeAt(i);
            count++;
        }
    if (count == 0) {
        Ori::Dlg::info(tr("All items are ok"));
        return;
    }
    s.setValue(KEY_COLOR_MAPS, colorMaps);
    Ori::Dlg::info(tr("Items deleted: %1").arg(count));
}

QString AppSettings::selectColorMapFile()
{
    Ori::Settings s;
    s.beginGroup(GROUP_PLOT);
    auto recentDir = s.value(KEY_COLOR_MAPS_DIR).toString();

    QString fileName = QFileDialog::getOpenFileName(qApp->activeWindow(),
        tr("Select Color Map"), recentDir, tr("CSV files (*.csv);;All files (*.*)"));
    if (fileName.isEmpty())
        return {};

    QFileInfo fi(fileName);
    s.setValue(KEY_COLOR_MAPS_DIR, fi.absoluteDir().absolutePath());
    fileName = fi.absoluteFilePath();

    auto colorMaps = s.value(KEY_COLOR_MAPS).toStringList();
    if (int i = colorMaps.indexOf(fileName); i >= 0)
        colorMaps.removeAt(i);
    colorMaps.insert(0, fileName);
    s.setValue(KEY_COLOR_MAPS, colorMaps);

    return fileName;
}
