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
    LOAD(idsInstallDir, String, "C:/Program Files/IDS/ids_peak");

    s.beginGroup("DeviceControl");
    LOAD(propChangeWheelSm, Int, 20);
    LOAD(propChangeWheelBig, Int, 100);
    LOAD(propChangeArrowSm, Int, 20);
    LOAD(propChangeArrowBig, Int, 100);
}

void AppSettings::save()
{
    Ori::Settings s;

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
        ConfigPage(0, tr("Device Control"), ":/toolbar/hardware"),
    };
    opts.items = {
        (new ConfigItemInt(0, tr("Small change by mouse wheel, %"), &propChangeWheelSm))
            ->withMinMax(1, 1000),
        (new ConfigItemInt(0, tr("Big change by mouse wheel, %"), &propChangeWheelBig))
            ->withMinMax(1, 1000)
            ->withHint(tr("Hold Control key for big change")),
        (new ConfigItemInt(0, tr("Small change by arrow keys, %"), &propChangeArrowSm))
            ->withMinMax(1, 1000),
        (new ConfigItemInt(0, tr("Big change by arrow keys, %"), &propChangeArrowBig))
            ->withMinMax(1, 1000)
            ->withHint(tr("Hold Control key for big change")),
    };
    if (ConfigDlg::edit(opts))
    {
        save();
        notify(&IAppSettingsListener::settingsChanged);
        return true;
    }
    return false;
}
