#include "DllCameraConfig.h"

#include "dialogs/OriConfigDlg.h"

#include <QSettings>

using namespace Ori::Dlg;

void DllCameraConfig::initDlg(Ori::Dlg::ConfigDlgOpts &opts, int maxPageId)
{
    if (!availablePixelFormats.isEmpty())
    {
        int pageHard = maxPageId + 1;
        opts.pages << ConfigPage(pageHard, tr("Hardware"), ":/toolbar/hardware").withHelpTopic("cam_settings_hard");
    
        auto pixelFormatSelector = new ConfigItemDropDown(pageHard, QString(), &pixelFormat);
        for (const auto &af : std::as_const(availablePixelFormats)) {
            pixelFormatSelector->withOption(af.code, af.descr);
        }
    
        opts.items
            << (new ConfigItemSection(pageHard, tr("Pixel format")))->withHint(tr("Reselect camera to apply"))
            << pixelFormatSelector;
    }
}

void DllCameraConfig::save(QSettings *s)
{
    s->setValue("hard.pixelFormat", pixelFormat);
}

void DllCameraConfig::load(QSettings *s)
{
    pixelFormat = s->value("hard.pixelFormat").toInt();
}
