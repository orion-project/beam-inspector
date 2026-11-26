#include "DllCameraConfig.h"

#include "dialogs/OriConfigDlg.h"

#include <QSettings>

using namespace Ori::Dlg;

void DllCameraConfig::initDlg(Ori::Dlg::ConfigDlgOpts &opts, int maxPageId)
{
    int pageHard = maxPageId + 1;
    int pageMisc = maxPageId + 3;

    if (!availablePixelFormats.isEmpty())
    {
        opts.pages << ConfigPage(pageHard, tr("Hardware"), ":/toolbar/hardware").withHelpTopic("cam_settings_hard");
    
        auto pixelFormatSelector = new ConfigItemDropDown(pageHard, QString(), &pixelFormat);
        for (const auto &af : std::as_const(availablePixelFormats)) {
            pixelFormatSelector->withOption(af.code, af.descr);
        }
    
        opts.items
            << (new ConfigItemSection(pageHard, tr("Pixel format")))->withHint(tr("Reselect camera to apply"))
            << pixelFormatSelector;
    }
    
    opts.pages << ConfigPage(pageMisc, tr("Options"), ":/toolbar/options").withHelpTopic("cam_settings_opts");
    opts.items
        << (new ConfigItemSection(pageMisc, tr("Frame brightness")))
            ->withHint(tr("Reselect camera to apply"))
        << new ConfigItemBool(pageMisc, tr("Show in results table"), &showBrightness)
        << new ConfigItemBool(pageMisc, tr("Save in measurement file"), &saveBrightness)
        << new ConfigItemSpace(pageMisc, 12);
    
    if (autoExpoEnabled)
    {
        opts.items 
            << new ConfigItemSection(pageMisc, tr("Autoexposure"))
            << (new ConfigItemInt(pageMisc, tr("Frame count for averaging"), &autoExpoAvgFrames))
                ->withMinMax(1, 100);
    }
}

void DllCameraConfig::save(QSettings *s)
{
    s->setValue("hard.pixelFormat", pixelFormat);
    s->setValue("autoExpo.targetLevel", autoExpoTargetLevel);
    s->setValue("autoExpo.avgFrames", autoExpoAvgFrames);
    s->setValue("showBrightness", showBrightness);
    s->setValue("saveBrightness", saveBrightness);

    s->beginWriteArray("propPresets", propPresets.size());
    for (int i = 0; i < propPresets.size(); i++) {
        s->setArrayIndex(i);
        const auto &preset = propPresets.at(i);
        for (auto it = preset.constBegin(); it != preset.constEnd(); it++)
            s->setValue(it.key(), it.value());
    }
    s->endArray();
}

void DllCameraConfig::load(QSettings *s)
{
    pixelFormat = s->value("hard.pixelFormat").toInt();
    autoExpoTargetLevel = s->value("autoExpo.targetLevel", 80).toInt();
    autoExpoAvgFrames = s->value("autoExpo.avgFrames", 4).toInt();
    showBrightness = s->value("showBrightness", false).toBool();
    saveBrightness = s->value("saveBrightness", false).toBool();

    propPresets.clear();
    int size = s->beginReadArray("propPresets");
    for (int i = 0; i < size; i++) {
        s->setArrayIndex(i);
        AnyRecord preset;
        foreach (const auto &key, s->allKeys())
            preset[key] = s->value(key);
        propPresets << preset;
    }
    s->endArray();
}
