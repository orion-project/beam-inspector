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
    LOAD(useConsole, Bool, false);

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

    s.beginGroup("Plot");
    LOAD(colorMap, String, "CET-L08");
}

void AppSettings::save()
{
    Ori::Settings s;
    SAVE(useConsole);

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

    s.beginGroup(GROUP_PLOT);
    SAVE(colorMap);
}

bool AppSettings::edit()
{
    ConfigDlgOpts opts;
    opts.objectName = "AppSettingsDlg";
    opts.pageIconSize = 32;
    opts.pages = {
        ConfigPage(cfgDev, tr("Device Control"), ":/toolbar/hardware")
            .withHelpTopic("app_settings_hard"),
    #ifdef WITH_IDS
        ConfigPage(cfgIds, tr("IDS Camera"), ":/toolbar/camera")
            .withHelpTopic("app_settings_ids"),
    #endif
        ConfigPage(cfgDbg, tr("Debug"), ":/toolbar/bug"),
    };
    opts.onHelpRequested = [](const QString &topic){ HelpSystem::topic(topic); };
    opts.items = {
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
    };
    if (ConfigDlg::edit(opts))
    {
        save();
        notify(&IAppSettingsListener::settingsChanged);
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
    auto currentMap = currentColorMap();
    for (const auto& fi : dir.entryInfoList()) {
        QString path = fi.absoluteFilePath();
        ColorMap map {
            .name = fi.baseName(),
            .file = path,
            .isCurrent = path == currentMap,
            .isExists = true,
        };
        QString descrFile = path.replace(QStringLiteral(".csv"), QStringLiteral(".ini"));
        if (QFileInfo::exists(descrFile)) {
            QSettings ini(descrFile, QSettings::IniFormat);
            ini.beginGroup("ColorMap");
            map.descr = ini.value("descr").toString();
        }
        result << map;
    }
    result << ColorMap();
    Ori::Settings s;
    s.beginGroup(GROUP_PLOT);
    for (const auto& file : s.value(KEY_COLOR_MAPS).toStringList()) {
        QFileInfo fi(file);
        QString path = fi.absoluteFilePath();
        result << ColorMap {
            .name = fi.baseName(),
            .file = path,
            .descr = path,
            .isCurrent = path == currentMap,
            .isExists = fi.exists(),
        };
    }
    return result;
}

QString AppSettings::currentColorMap()
{
    QFileInfo fi(colorMap);
    if (!fi.isAbsolute())
        fi = QFileInfo(colorMapDir() + '/' + colorMap);
    if (fi.suffix().isEmpty())
        fi = QFileInfo(fi.absoluteFilePath() + QStringLiteral(".csv"));
    return fi.absoluteFilePath();
}

void AppSettings::setCurrentColorMap(const QString& fileName)
{
    colorMap = fileName;
    save();
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
