#include "Camera.h"

#include "app/HelpSystem.h"

#include "dialogs/OriConfigDlg.h"
#include "helpers/OriDialogs.h"
#include "helpers/OriLayouts.h"
#include "helpers/OriWidgets.h"
#include "tools/OriSettings.h"
#include "widgets/OriValueEdit.h"

#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>

using namespace Ori::Dlg;
using namespace Ori::Layouts;

Camera::Camera(PlotIntf *plot, TableIntf *table, const char* configGroup) :
    _plot(plot), _table(table), _configGroup(configGroup)
{
}

QString Camera::resolutionStr() const
{
    return QStringLiteral("%1 × %2 × %3bit").arg(width()).arg(height()).arg(bpp());
}

void Camera::loadConfig()
{
    Ori::Settings s;
    s.beginGroup(_configGroup);
    _config.load(s.settings());
    loadConfigMore(s.settings());
}

void Camera::saveConfig(bool saveMore)
{
    Ori::Settings s;
    s.beginGroup(_configGroup);
    _config.save(s.settings());
    if (saveMore)
        saveConfigMore(s.settings());
}

bool Camera::editConfig(int page)
{
    ConfigDlgOpts opts;
    opts.currentPageId = page;
    opts.objectName = "CamConfigDlg";
    opts.pageIconSize = 32;
    opts.pages = {
        ConfigPage(cfgPlot, qApp->tr("Plot"), ":/toolbar/zoom_sensor")
            .withHelpTopic("cam_settings_plot"),
        ConfigPage(cfgBgnd, qApp->tr("Background"), ":/toolbar/beam")
            .withSpacing(12)
            .withHelpTopic("cam_settings_bgnd"),
        ConfigPage(cfgCentr, qApp->tr("Centroid"), ":/toolbar/centroid")
            .withSpacing(12)
            .withLongTitle(qApp->tr("Centroid Calculation"))
            .withHelpTopic("cam_settings_centr"),
        ConfigPage(cfgRoi, qApp->tr("ROI"), ":/toolbar/roi")
            .withSpacing(12)
            .withLongTitle(qApp->tr("Region of Interest"))
            .withHelpTopic("cam_settings_roi"),
    };
    opts.onHelpRequested = [](const QString &topic){ HelpSystem::topic(topic); };
    auto hardScale = sensorScale();
    bool useSensorScale = !_config.plot.customScale.on;
    bool useCustomScale = _config.plot.customScale.on;
    bool scaleFullRange = _config.plot.fullRange;
    bool scaleDataRange = !_config.plot.fullRange;
    double cornerFraction = _config.bgnd.corner * 100;
    int roiPixelLeft = qRound(double(width()) * _config.roi.left);
    int roiPixelRight = qRound(double(width()) * _config.roi.right);
    int roiPixelTop = qRound(double(height()) * _config.roi.top);
    int roiPixelBottom = qRound(double(height()) * _config.roi.bottom);
    opts.items = {
        new ConfigItemBool(cfgPlot, qApp->tr("Normalize data"), &_config.plot.normalize),
        new ConfigItemSpace(cfgPlot, 12),
        (new ConfigItemBool(cfgPlot, qApp->tr("Colorize over full range"), &scaleFullRange))
            ->withRadioGroup("colorize"),
        (new ConfigItemBool(cfgPlot, qApp->tr("Colorize over data range"), &scaleDataRange))
            ->withRadioGroup("colorize"),
        new ConfigItemSpace(cfgPlot, 12),
        new ConfigItemBool(cfgPlot, qApp->tr("Rescale pixels"), &_config.plot.rescale),
        (new ConfigItemBool(cfgPlot, qApp->tr("Use hardware scale"), &useSensorScale))
            ->setDisabled(!hardScale.on)
            ->withRadioGroup("pixel_scale")
            ->withHint(hardScale.on
                ? QString("1px = %1 %2").arg(hardScale.factor).arg(hardScale.unit)
                : qApp->tr("Camera does not provide meta data")),
        (new ConfigItemBool(cfgPlot, qApp->tr("Use custom scale"), &useCustomScale))
            ->withRadioGroup("pixel_scale"),
        new ConfigItemReal(cfgPlot, qApp->tr("1px equals to"), &_config.plot.customScale.factor),
        (new ConfigItemStr(cfgPlot, qApp->tr("of units"), &_config.plot.customScale.unit))
            ->withAlignment(Qt::AlignRight),

        new ConfigItemBool(cfgBgnd, qApp->tr("Subtract background"), &_config.bgnd.on),
        (new ConfigItemReal(cfgBgnd, qApp->tr("Corner Fraction %"), &cornerFraction))
            ->withHint(qApp->tr("ISO 11146 recommends 2-5%"), false),
        (new ConfigItemReal(cfgBgnd, qApp->tr("Noise Factor"), &_config.bgnd.noise))
            ->withHint(qApp->tr("ISO 11146 recommends 2-4"), false),

        (new ConfigItemReal(cfgCentr, qApp->tr("Mask Diameter"), &_config.bgnd.mask))
            ->withHint(qApp->tr("ISO 11146 recommends 3"), false),
        (new ConfigItemInt(cfgCentr, qApp->tr("Max Iterations"), &_config.bgnd.iters))
            ->withMinMax(0, 50),
        new ConfigItemReal(cfgCentr, qApp->tr("Precision"), &_config.bgnd.precision),

        (new ConfigItemBool(cfgRoi, qApp->tr("Use region"), &_config.roi.on))
            ->withHint(qApp->tr(
                "These are raw pixels values. "
                "Use the menu command <b>Camera ► Edit ROI</b> "
                "to change region interactively in scaled units"), true),
        (new ConfigItemInt(cfgRoi, qApp->tr("Left"), &roiPixelLeft))
            ->withMinMax(0, width()),
        (new ConfigItemInt(cfgRoi, qApp->tr("Top"), &roiPixelTop))
            ->withMinMax(0, height()),
        (new ConfigItemInt(cfgRoi, qApp->tr("Right"), &roiPixelRight))
            ->withMinMax(0, width()),
        (new ConfigItemInt(cfgRoi, qApp->tr("Bottom"), &roiPixelBottom))
            ->withMinMax(0, height())
    };
    initConfigMore(opts);
    if (ConfigDlg::edit(opts))
    {
        _config.plot.fullRange = scaleFullRange;
        _config.plot.customScale.on = useCustomScale;
        _config.bgnd.corner = cornerFraction / 100.0;
        _config.roi.left = double(roiPixelLeft)/double(width());
        _config.roi.right = double(roiPixelRight)/double(width());
        _config.roi.top = double(roiPixelTop)/double(height());
        _config.roi.bottom = double(roiPixelBottom)/double(height());
        _config.roi.fix();
        saveConfig(true);
        return true;
    }
    return false;
}

void Camera::setAperture(const RoiRect &a)
{
    _config.roi = a;
    if (_config.roi.on)
        _config.roi.fix();
    saveConfig();
    raisePowerWarning();
}

void Camera::toggleAperture(bool on)
{
    _config.roi.on = on;
    if (on && _config.roi.isZero()) {
        _config.roi.left = 0.25;
        _config.roi.top = 0.25;
        _config.roi.right = 0.75;
        _config.roi.bottom = 0.75;
    }
    saveConfig();
    raisePowerWarning();
}

bool Camera::isRoiValid() const
{
    return _config.roi.isValid();
}

PixelScale Camera::pixelScale() const
{
    if (!_config.plot.rescale)
        return {};
    if (_config.plot.customScale.on)
        return _config.plot.customScale;
    return sensorScale();
}

bool Camera::setupPowerMeter()
{
    auto cbEnable = new QCheckBox(qApp->tr("Show power"));
    auto seFrames = Ori::Gui::spinBox(1, 10);
    auto edPower = new Ori::Widgets::ValueEdit;
    auto cbFactor = new QComboBox;
    cbFactor->addItem(qApp->tr("W"));
    cbFactor->addItem(qApp->tr("mW"));
    cbFactor->addItem(qApp->tr("uW"));
    cbFactor->addItem(qApp->tr("nW"));

    bool wasOn = _config.power.on;
    cbEnable->setChecked(_config.power.on);
    seFrames->setValue(_config.power.avgFrames);
    edPower->setValue(_config.power.power);
    cbFactor->setCurrentIndex(_config.power.decimalFactor / -3);

    auto w = LayoutV({
        cbEnable,
        SpaceV(1),
        qApp->tr("Current brightness\naveraged over frames:"),
        seFrames,
        SpaceV(1),
        qApp->tr("Corresponds to power:"),
        LayoutH({edPower, cbFactor}),
    }).setMargin(0).makeWidgetAuto();
    bool ok = Dialog(w)
        .withHelpIcon(":/ori_images/help")
        .withOnHelp([]{ HelpSystem::topic("power_setup"); })
        .withContentToButtonsSpacingFactor(3)
        .windowModal()
        .exec();
    if (ok) {
        _config.power.on = cbEnable->isChecked();
        _config.power.avgFrames = std::clamp(seFrames->value(), PowerMeter::minAvgFrames, PowerMeter::maxAvgFrames);
        _config.power.power = qMax(edPower->value(), 0.0);
        _config.power.decimalFactor = cbFactor->currentIndex() * -3;
        saveConfig();
        togglePowerMeter();
        if (resultRowsChanged && wasOn != _config.power.on)
            resultRowsChanged();
    }
    return ok;
}
