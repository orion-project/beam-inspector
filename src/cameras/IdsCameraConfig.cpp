#include "IdsCameraConfig.h"

#ifdef WITH_IDS

#include "app/AppSettings.h"
#include "cameras/IdsCamera.h"
#include "cameras/IdsLib.h"

#include "dialogs/OriConfigDlg.h"
#include "helpers/OriLayouts.h"

#include <QComboBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>

using namespace Ori::Dlg;
using namespace Ori::Layouts;

//------------------------------------------------------------------------------
//                               ConfigEditorXY
//------------------------------------------------------------------------------

class ConfigEditorFactorXY : public ConfigItemEditor
{
public:
    ConfigEditorFactorXY(IdsCameraConfig::FactorXY *factor, ConfigEditorFactorXY *other) : ConfigItemEditor(), factor(factor), other(other)
    {
        comboX = new QComboBox;
        comboY = new QComboBox;
        fillCombo(comboX, factor->xs, factor->x);
        fillCombo(comboY, factor->ys, factor->y);
        connect(comboX, &QComboBox::activated, this, &ConfigEditorFactorXY::updateOther);
        connect(comboY, &QComboBox::activated, this, &ConfigEditorFactorXY::updateOther);
        LayoutH({ QString("X:"), comboX, SpaceH(4), QString("Y:"), comboY, Stretch() }).useFor(this);
        if (other) {
            other->other = this;
            other->updateOther();
        }
        updateOther();
    }

    uint32_t factorX() const { return qMax(1u, comboX->currentData().toUInt()); }
    uint32_t factorY() const { return qMax(1u, comboY->currentData().toUInt()); }
    bool on() const { return factorX() > 1 or factorY() > 1; }

    void collect() override
    {
        factor->x = factorX();
        factor->y = factorY();
    }

    void fillCombo(QComboBox *combo, const QList<quint32> &items, quint32 cur)
    {
        int idx = -1;
        for (int i = 0; i < items.size(); i++) {
            quint32 v = items[i];
            combo->addItem(QString::number(v), v);
            if (v == cur)
                idx = i;
        }
        if (idx >= 0)
            combo->setCurrentIndex(idx);
    }

    void updateOther() {
        if (!other) return;
        bool thisOn = on();
        other->comboX->setDisabled(thisOn);
        other->comboY->setDisabled(thisOn);
    }

    IdsCameraConfig::FactorXY *factor;
    ConfigEditorFactorXY *other = nullptr;
    QComboBox *comboX, *comboY;
};

//------------------------------------------------------------------------------
//                              IdsCameraConfig
//------------------------------------------------------------------------------

void IdsCameraConfig::initDlg(peak_camera_handle hCam, Ori::Dlg::ConfigDlgOpts &opts, int maxPageId)
{
    int pageHard = maxPageId + 1;
    int pageInfo = maxPageId + 2;
    int pageMisc = maxPageId + 3;
    opts.pages << ConfigPage(pageHard, tr("Hardware"), ":/toolbar/hardware").withHelpTopic("cam_settings_hard");
#ifdef SHOW_ALL_PIXEL_FORMATS
    auto formatsItem = new ConfigItemDropDown(pageHard, tr("Pixel format"), &pixelFormat);
    formatsItem->withHint(tr("Reselect camera to apply"));
    if (AppSettings::instance().showAllPixelFormats)
    {
        for (const auto &f : std::as_const(camFormats))
            formatsItem->withOption(f.code, f.descr);
    }
    else
    {
        {
            auto f = IdsCamera::supportedPixelFormat_Mono8();
            formatsItem->withOption(f.code, f.descr);
        }
        {
            auto f = IdsCamera::supportedPixelFormat_Mono10G40();
            formatsItem->withOption(f.code, f.descr);
        }
        {
            auto f = IdsCamera::supportedPixelFormat_Mono12G24();
            formatsItem->withOption(f.code, f.descr);
        }
    }
    opts.items << formatsItem;
#else
    bpp8 = bpp == 8;
    bpp10 = bpp == 10;
    bpp12 = bpp == 12;
    opts.items
        << (new ConfigItemSection(pageHard, tr("Pixel format")))
            ->withHint(tr("Reselect camera to apply"))
        << (new ConfigItemBool(pageHard, tr("8 bit"), &bpp8))
            ->setDisabled(!supportedBpp.contains(8))->withRadioGroup("pixel_format")
        << (new ConfigItemBool(pageHard, tr("10 bit"), &bpp10))
            ->setDisabled(!supportedBpp.contains(10))->withRadioGroup("pixel_format")
        << (new ConfigItemBool(pageHard, tr("12 bit"), &bpp12))
            ->setDisabled(!supportedBpp.contains(12))->withRadioGroup("pixel_format")
#endif
    ;

    opts.items
        << new ConfigItemSpace(pageHard, 12)
        << (new ConfigItemSection(pageHard, tr("Resolution reduction")))->withHint(tr("Reselect camera to apply"));
    ConfigEditorFactorXY *binnigEditor = nullptr;
    if (binning.configurable) {
        binnigEditor = new ConfigEditorFactorXY(&binning, nullptr);
        opts.items << (new ConfigItemCustom(pageHard, tr("Binning"), binnigEditor))
            ->withHint(tr("Some models support only combined binning mode, please check the spec"));
    } else {
        opts.items << (new ConfigItemEmpty(pageHard, tr("Binning")))->withHint(tr("Is not configurable"));
    }
    if (decimation.configurable) {
        opts.items << new ConfigItemCustom(pageHard, tr("Decimation"), new ConfigEditorFactorXY(&decimation, binnigEditor));
    } else {
        opts.items << (new ConfigItemEmpty(pageHard, tr("Decimation")))->withHint(tr("Is not configurable"));
    }

    if (!intoRequested) {
        intoRequested = true;
        infoModelName = IDS.gfaGetStr(hCam, "DeviceModelName");
        infoFamilyName = IDS.gfaGetStr(hCam, "DeviceFamilyName");
        infoSerialNum = IDS.gfaGetStr(hCam, "DeviceSerialNumber");
        infoVendorName = IDS.gfaGetStr(hCam, "DeviceVendorName");
        infoManufacturer = IDS.gfaGetStr(hCam, "DeviceManufacturerInfo");
        infoDeviceVer = IDS.gfaGetStr(hCam, "DeviceVersion");
        infoFirmwareVer = IDS.gfaGetStr(hCam, "DeviceFirmwareVersion");
    }
    opts.pages << ConfigPage(pageInfo, tr("Info"), ":/toolbar/info");
    opts.items
        << (new ConfigItemStr(pageInfo, tr("Model name"), &infoModelName))->withReadOnly()
        << (new ConfigItemStr(pageInfo, tr("Family name"), &infoFamilyName))->withReadOnly()
        << (new ConfigItemStr(pageInfo, tr("Serial number"), &infoSerialNum))->withReadOnly()
        << (new ConfigItemStr(pageInfo, tr("Vendor name"), &infoVendorName))->withReadOnly()
        << (new ConfigItemStr(pageInfo, tr("Manufacturer info"), &infoManufacturer))->withReadOnly()
        << (new ConfigItemStr(pageInfo, tr("Device version"), &infoDeviceVer))->withReadOnly()
        << (new ConfigItemStr(pageInfo, tr("Firmware version"), &infoFirmwareVer))->withReadOnly()
    ;

    opts.pages << ConfigPage(pageMisc, tr("Options"), ":/toolbar/options").withHelpTopic("cam_settings_opts");
    opts.items
        << (new ConfigItemSection(pageMisc, tr("Frame brightness")))
            ->withHint(tr("Reselect camera to apply"))
        << new ConfigItemBool(pageMisc, tr("Show in results table"), &showBrightness)
        << new ConfigItemBool(pageMisc, tr("Save in measurement file"), &saveBrightness)
        << new ConfigItemSpace(pageMisc, 12)
        << new ConfigItemSection(pageMisc, tr("Autoexposure"))
        << (new ConfigItemInt(pageMisc, tr("Frame count for averaging"), &autoExpFramesAvg))
            ->withMinMax(1, 100)
    ;
#ifdef SHOW_ALL_PIXEL_FORMATS
    oldPixelFormat = pixelFormat;
#else
    oldBpp = bpp;
#endif
    oldBinning = binning;
    oldDecimation = decimation;
    hasPowerWarning = false;
}

void IdsCameraConfig::save(QSettings *s)
{
#ifdef SHOW_ALL_PIXEL_FORMATS
    s->setValue("hard.pixelFormat", pixelFormat);
    if (pixelFormat == IdsCamera::supportedPixelFormat_Mono12G24())
        bpp = 12;
    else if (pixelFormat == IdsCamera::supportedPixelFormat_Mono10G40())
        bpp = 10;
    else if (pixelFormat == IdsCamera::supportedPixelFormat_Mono8())
        bpp = 8;
    else {
        // For formats we can not decode, an empty 8-bit buffer will be used
        // for calculations (see IdsCamera::initPixelFormat)
        bpp = 8;
    }
#else
    bpp = bpp12 ? 12 : bpp10 ? 10 : 8;
#endif
    s->setValue("hard.bpp", bpp);
    s->setValue("hard.binning.x", binning.x);
    s->setValue("hard.binning.y", binning.y);
    s->setValue("hard.decimation.x", decimation.x);
    s->setValue("hard.decimation.y", decimation.y);
    s->setValue("showBrightness", showBrightness);
    s->setValue("saveBrightness", saveBrightness);
    s->setValue("autoExpLevel", autoExpLevel);
    s->setValue("autoExpFramesAvg", autoExpFramesAvg);
    s->setValue("fpsLock", fpsLock);

    hasPowerWarning = 
#ifdef SHOW_ALL_PIXEL_FORMATS
        oldPixelFormat != pixelFormat ||
#else
        oldBpp != bpp ||
#endif
        oldBinning.x != binning.x || oldBinning.y != binning.y ||
        oldDecimation.x != decimation.x || oldDecimation.y != decimation.y;

    s->beginWriteArray("exp_presets", expPresets.size());
    for (int i = 0; i < expPresets.size(); i++) {
        s->setArrayIndex(i);
        const auto &preset = expPresets.at(i);
        for (auto it = preset.constBegin(); it != preset.constEnd(); it++)
            s->setValue(it.key(), it.value());
    }
    s->endArray();
}

void IdsCameraConfig::load(QSettings *s)
{
    bpp = s->value("hard.bpp", 8).toInt();
#ifdef SHOW_ALL_PIXEL_FORMATS
    pixelFormat = s->value("hard.pixelFormat").toInt();
    if (pixelFormat <= 0) {
        if (bpp == 12)
            pixelFormat = IdsCamera::supportedPixelFormat_Mono12G24().code;
        else if (bpp == 10)
            pixelFormat = IdsCamera::supportedPixelFormat_Mono10G40().code;
        else
            pixelFormat = IdsCamera::supportedPixelFormat_Mono8().code;
    }
#endif
    binning.x = qMax(1u, s->value("hard.binning.x", 1).toUInt());
    binning.y = qMax(1u, s->value("hard.binning.y", 1).toUInt());
    decimation.x = qMax(1u, s->value("hard.decimation.x", 1).toUInt());
    decimation.y = qMax(1u, s->value("hard.decimation.y", 1).toUInt());
    if (binning.on() && decimation.on())
        decimation.reset();
    showBrightness = s->value("showBrightness", false).toBool();
    saveBrightness = s->value("saveBrightness", false).toBool();
    autoExpLevel = s->value("autoExpLevel").toInt();
    autoExpFramesAvg = s->value("autoExpFramesAvg", 4).toInt();
    fpsLock = s->value("fpsLock", 0).toDouble();

    AnyRecords presets;
    int size = s->beginReadArray("exp_presets");
    for (int i = 0; i < size; i++) {
        s->setArrayIndex(i);
        AnyRecord preset;
        foreach (const auto &key, s->allKeys())
            preset[key] = s->value(key);
        presets << preset;
    }
    s->endArray();
    expPresets = presets;
}

void IdsCameraConfig::saveExpPresets(QJsonObject &root)
{
    QJsonArray items;
    foreach (auto const &preset, expPresets) {
        QJsonObject item;
        for (auto it = preset.constBegin(); it != preset.constEnd(); it++)
            item[it.key()] = QJsonValue::fromVariant(it.value());
        items << item;
    }
    root["exposure_presets"] = items;
}

void IdsCameraConfig::loadExpPresets(QJsonObject &root)
{
    expPresets.clear();
    auto items = root["exposure_presets"].toArray();
    for (auto it = items.begin(); it != items.end(); it++) {
        auto item = it->toObject();
        AnyRecord preset;
        foreach (const auto &key, item.keys())
            preset[key] = item[key].toVariant();
        expPresets << preset;
    }
}

#endif // WITH_IDS
