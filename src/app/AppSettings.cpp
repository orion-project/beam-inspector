#include "AppSettings.h"

#include "dialogs/OriConfigDlg.h"
#include "tools/OriSettings.h"

using namespace Ori::Dlg;

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
    LOAD(idsBitsPerPixel, Int, 8);
#endif

    s.beginGroup("DeviceControl");
    LOAD(propChangeWheelSm, Int, 20);
    LOAD(propChangeWheelBig, Int, 100);
    LOAD(propChangeArrowSm, Int, 20);
    LOAD(propChangeArrowBig, Int, 100);
}

void AppSettings::save()
{
    Ori::Settings s;
    SAVE(useConsole);

#ifdef WITH_IDS
    s.beginGroup("IdsCamera");
    SAVE(idsEnabled);
    SAVE(idsSdkDir);
    SAVE(idsBitsPerPixel);
#endif

    s.beginGroup("DeviceControl");
    SAVE(propChangeWheelSm);
    SAVE(propChangeWheelBig);
    SAVE(propChangeArrowSm);
    SAVE(propChangeArrowBig);
}

bool AppSettings::edit()
{
    ConfigDlgOpts opts;
    opts.objectName = "AppSettingsDlg";
    opts.pageIconSize = 32;
    opts.pages = {
        ConfigPage(cfgDev, tr("Device Control"), ":/toolbar/hardware"),
    #ifdef WITH_IDS
        ConfigPage(cfgIds, tr("IDS Camera"), ":/toolbar/camera"),
    #endif
        ConfigPage(cfgDbg, tr("Debug"), ":/toolbar/bug"),
    };
    #ifdef WITH_IDS
    bool pixelFormat8 = idsBitsPerPixel == 8;
    bool pixelFormat10 = idsBitsPerPixel == 10;
    bool pixelFormat12 = idsBitsPerPixel == 12;
    #endif
    opts.items = {
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
        new ConfigItemSpace(cfgIds, 12),
        (new ConfigItemSection(cfgIds, tr("Pixel format")))
            ->withHint(tr("Reselect camera to apply")),
        (new ConfigItemBool(cfgIds, tr("Prefer 8 bit"), &pixelFormat8))
            ->withRadioGroup(0),
        (new ConfigItemBool(cfgIds, tr("Prefer 10 bit"), &pixelFormat10))
            ->withRadioGroup(0),
        (new ConfigItemBool(cfgIds, tr("Prefer 12 bit"), &pixelFormat12))
            ->withRadioGroup(0),
    #endif
        new ConfigItemBool(cfgDbg, tr("Show log window (restart required)"), &useConsole),
    };
    if (ConfigDlg::edit(opts))
    {
    #ifdef WITH_IDS
        if (pixelFormat12)
            idsBitsPerPixel = 12;
        else if (pixelFormat10)
            idsBitsPerPixel = 10;
        else idsBitsPerPixel = 8;
    #endif
        save();
        notify(&IAppSettingsListener::settingsChanged);
        return true;
    }
    return false;
}
