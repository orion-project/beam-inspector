#include "Camera.h"

#include "dialogs/OriConfigDlg.h"
#include "tools/OriSettings.h"

#include <QApplication>
#include <QLabel>

using namespace Ori::Dlg;

Camera::Camera(PlotIntf *plot, TableIntf *table, const char* configGroup) :
    _plot(plot), _table(table), _configGroup(configGroup)
{
    loadConfig();
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
}

void Camera::saveConfig()
{
    Ori::Settings s;
    s.beginGroup(_configGroup);
    _config.save(s.settings());
}

bool Camera::editConfig(int page)
{
    ConfigDlgOpts opts;
    opts.currentPageId = page;
    opts.objectName = "CamConfigDlg";
    opts.pageIconSize = 32;
    opts.pages = {
        ConfigPage(cfgPlot, qApp->tr("Plot"), ":/toolbar/zoom_sensor"),
        ConfigPage(cfgBgnd, qApp->tr("Background"), ":/toolbar/beam").withSpacing(12),
        ConfigPage(cfgRoi, qApp->tr("ROI"), ":/toolbar/roi").withSpacing(12)
            .withLongTitle(qApp->tr("Region of Interest")),
    };
    auto hardScale = sensorScale();
    bool useSensorScale = !_config.plot.customScale.on;
    bool useCustomScale = _config.plot.customScale.on;
    bool scaleFullRange = _config.plot.fullRange;
    bool scaleDataRange = !_config.plot.fullRange;
    double cornerFraction = _config.bgnd.corner * 100;
    opts.items = {
        new ConfigItemBool(cfgPlot, qApp->tr("Normalize data"), &_config.plot.normalize),
        new ConfigItemSpace(cfgPlot, 12),
        (new ConfigItemBool(cfgPlot, qApp->tr("Colorize over full range"), &scaleFullRange))
            ->withRadioGroup(1),
        (new ConfigItemBool(cfgPlot, qApp->tr("Colorize over data range"), &scaleDataRange))
            ->withRadioGroup(1),
        new ConfigItemSpace(cfgPlot, 12),
        new ConfigItemBool(cfgPlot, qApp->tr("Rescale pixels"), &_config.plot.rescale),
        (new ConfigItemBool(cfgPlot, qApp->tr("Use hardware scale"), &useSensorScale))
            ->setDisabled(!hardScale.on)
            ->withRadioGroup(0)
            ->withHint(hardScale.on
                ? QString("1px = %1 %2").arg(hardScale.factor).arg(hardScale.unit)
                : qApp->tr("Camera does not provide meta data")),
        (new ConfigItemBool(cfgPlot, qApp->tr("Use custom scale"), &useCustomScale))
            ->withRadioGroup(0),
        new ConfigItemReal(cfgPlot, qApp->tr("1px equals to"), &_config.plot.customScale.factor),
        (new ConfigItemStr(cfgPlot, qApp->tr("of units"), &_config.plot.customScale.unit))
            ->withAlignment(Qt::AlignRight),

        new ConfigItemBool(cfgBgnd, qApp->tr("Subtract background"), &_config.bgnd.on),
        (new ConfigItemInt(cfgBgnd, qApp->tr("Max iterations"), &_config.bgnd.iters))
            ->withMinMax(0, 50),
        new ConfigItemReal(cfgBgnd, qApp->tr("Precision"), &_config.bgnd.precision),
        (new ConfigItemReal(cfgBgnd, qApp->tr("Corner Fraction %"), &cornerFraction))
            ->withHint(qApp->tr("ISO 11146 recommends 2-5%"), false),
        (new ConfigItemReal(cfgBgnd, qApp->tr("Noise Factor"), &_config.bgnd.noise))
            ->withHint(qApp->tr("ISO 11146 recommends 2-4"), false),
        (new ConfigItemReal(cfgBgnd, qApp->tr("Mask Diameter"), &_config.bgnd.mask))
            ->withHint(qApp->tr("ISO 11146 recommends 3"), false),

        (new ConfigItemBool(cfgRoi, qApp->tr("Use region"), &_config.roi.on))
            ->withHint(qApp->tr(
                "These are raw pixels values. "
                "Use the menu command <b>Camera ► Edit ROI</b> "
                "to change region interactively in scaled units"), true),
        (new ConfigItemInt(cfgRoi, qApp->tr("Left"), &_config.roi.x1))
            ->withMinMax(0, width()),
        (new ConfigItemInt(cfgRoi, qApp->tr("Top"), &_config.roi.y1))
            ->withMinMax(0, height()),
        (new ConfigItemInt(cfgRoi, qApp->tr("Right"), &_config.roi.x2))
            ->withMinMax(0, width()),
        (new ConfigItemInt(cfgRoi, qApp->tr("Bottom"), &_config.roi.y2))
            ->withMinMax(0, height())
    };
    if (ConfigDlg::edit(opts))
    {
        _config.plot.fullRange = scaleFullRange;
        _config.plot.customScale.on = useCustomScale;
        _config.bgnd.corner = cornerFraction / 100.0;
        _config.roi.fix(width(), height());
        saveConfig();
        return true;
    }
    return false;
}

void Camera::setAperture(const RoiRect &a)
{
    _config.roi = a;
    if (_config.roi.on)
        _config.roi.fix(width(), height());
    saveConfig();
}

void Camera::toggleAperture(bool on)
{
    _config.roi.on = on;
    if (on && _config.roi.isZero()) {
        _config.roi.x1 = width() / 4;
        _config.roi.y1 = height() / 4;
        _config.roi.x2 = width() / 4 * 3;
        _config.roi.y2 = height() / 4 * 3;
    }
    saveConfig();
}

bool Camera::isRoiValid() const
{
    return _config.roi.isValid(width(), height());
}

PixelScale Camera::pixelScale() const
{
    if (!_config.plot.rescale)
        return {};
    if (_config.plot.customScale.on)
        return _config.plot.customScale;
    return sensorScale();
}
