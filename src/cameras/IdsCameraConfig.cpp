#include "IdsCameraConfig.h"

#ifdef WITH_IDS

#include "cameras/IdsLib.h"

#include "dialogs/OriConfigDlg.h"
#include "helpers/OriLayouts.h"
#include "tools/OriSettings.h"

#include <QComboBox>

using namespace Ori::Dlg;
using namespace Ori::Layouts;

//------------------------------------------------------------------------------
//                               ConfigEditorXY
//------------------------------------------------------------------------------

class ConfigEditorXY : public ConfigItemEditor
{
public:
    ConfigEditorXY(const QList<quint32> &xs, const QList<quint32> &ys, quint32 *x, quint32 *y) : ConfigItemEditor(), x(x), y(y)
    {
        comboX = new QComboBox;
        comboY = new QComboBox;
        fillCombo(comboX, xs, *x);
        fillCombo(comboY, ys, *y);
        LayoutH({ QString("X:"), comboX, SpaceH(4), QString("Y:"), comboY, Stretch() }).useFor(this);
    }

    void collect() override
    {
        *x = qMax(1u, comboX->currentData().toUInt());
        *y = qMax(1u, comboY->currentData().toUInt());
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

    quint32 *x, *y;
    QComboBox *comboX, *comboY;
};

//------------------------------------------------------------------------------
//                              IdsCameraConfig
//------------------------------------------------------------------------------

void IdsCameraConfig::initDlg(peak_camera_handle hCam, Ori::Dlg::ConfigDlgOpts &opts, int maxPageId)
{
    int pageHard = maxPageId + 1;
    int pageInfo = maxPageId + 2;
    bpp8 = bpp == 8;
    bpp10 = bpp == 10;
    bpp12 = bpp == 12;
    opts.pages << ConfigPage(pageHard, tr("Hardware"), ":/toolbar/hardware");
    opts.items
        << (new ConfigItemSection(pageHard, tr("Pixel format")))
            ->withHint(tr("Reselect camera to apply"))
        << (new ConfigItemBool(pageHard, tr("8 bit"), &bpp8))
            ->setDisabled(!supportedBpp.contains(8))->withRadioGroup("pixel_format")
        << (new ConfigItemBool(pageHard, tr("10 bit"), &bpp10))
            ->setDisabled(!supportedBpp.contains(10))->withRadioGroup("pixel_format")
        << (new ConfigItemBool(pageHard, tr("12 bit"), &bpp12))
            ->setDisabled(!supportedBpp.contains(12))->withRadioGroup("pixel_format")
    ;

    opts.items
        << new ConfigItemSpace(pageHard, 12)
        << (new ConfigItemSection(pageHard, tr("Resolution reduction")))
            ->withHint(tr("Reselect camera to apply"));
    if (binningX > 0)
        opts.items << new ConfigItemCustom(pageHard, tr("Binning"), new ConfigEditorXY(
            binningsX, binningsY, &binningX, &binningY));
    else
        opts.items << (new ConfigItemEmpty(pageHard, tr("Binning")))
            ->withHint(tr("Is not configurable"));
    if (decimX > 0)
        opts.items << new ConfigItemCustom(pageHard, tr("Decimation"), new ConfigEditorXY(
            decimsX, decimsY, &decimX, &decimY));
    else
        opts.items << (new ConfigItemEmpty(pageHard, tr("Decimation")))
            ->withHint(tr("Is not configurable"));

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
}

void IdsCameraConfig::save(const QString& group)
{
    Ori::Settings s;
    s.beginGroup(group);
    s.setValue("hard.bpp", bpp12 ? 12 : bpp10 ? 10 : 8);
    s.setValue("hard.binning.x", binningX);
    s.setValue("hard.binning.y", binningY);
    s.setValue("hard.decim.x", decimX);
    s.setValue("hard.decim.y", decimY);
}

void IdsCameraConfig::load(const QString& group)
{
    Ori::Settings s;
    s.beginGroup(group);
    bpp = s.value("hard.bpp", 8).toInt();
    binningX = s.value("hard.binning.x").toUInt();
    binningY = s.value("hard.binning.y").toUInt();
    decimX = s.value("hard.decim.x").toUInt();
    decimY = s.value("hard.decim.y").toUInt();
}

#endif // WITH_IDS
