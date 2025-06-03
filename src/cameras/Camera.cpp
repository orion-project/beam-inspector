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

Camera::Camera(PlotIntf *plot, TableIntf *table, StabilityIntf *stabil, const char* configGroup) :
    _plot(plot), _table(table), _stabil(stabil), _configGroup(configGroup)
{
}

QString Camera::resolutionStr() const
{
    return QStringLiteral("%1 × %2 × %3bit").arg(width()).arg(height()).arg(bpp());
}

QString Camera::formatBrightness(double v) const
{
    if (_config.plot.normalize)
        return QString::number(v, 'f', 3);
    return QString::number(v, 'f', 0);
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
            .withHelpTopic("cam_settings_plot")
    };
    if (canMavg()) {
        opts.pages << ConfigPage(cfgTable, qApp->tr("Table"), ":/toolbar/table")
            .withSpacing(12)
            .withHelpTopic("cam_settings_table");
    }
    opts.pages
        << ConfigPage(cfgBgnd, qApp->tr("Background"), ":/toolbar/beam")
            .withSpacing(12)
            .withHelpTopic("cam_settings_bgnd")
        << ConfigPage(cfgCentr, qApp->tr("Centroid"), ":/toolbar/centroid")
            .withSpacing(12)
            .withLongTitle(qApp->tr("Centroid Calculation"))
            .withHelpTopic("cam_settings_centr")
        << ConfigPage(cfgRoi, qApp->tr("ROI"), ":/toolbar/roi")
            .withSpacing(12)
            .withLongTitle(qApp->tr("Region of Interest"))
            .withHelpTopic("cam_settings_roi")
    ;
    opts.onHelpRequested = [](const QString &topic){ HelpSystem::topic(topic); };
    auto hardScale = sensorScale();
    bool rescalePlot = _config.plot.rescale;
    bool useSensorScale = !_config.plot.customScale.on;
    bool useCustomScale = _config.plot.customScale.on;
    // Using of hard scale is checked but camera doesn't support it
    if (rescalePlot && useSensorScale && !hardScale.on)
        rescalePlot = false, useSensorScale = false, useCustomScale = true;
    bool scaleFullRange = _config.plot.fullRange;
    bool scaleDataRange = !_config.plot.fullRange;
    double cornerFraction = _config.bgnd.corner * 100;
    int roiPixelLeft = qRound(double(width()) * _config.roi.left);
    int roiPixelRight = qRound(double(width()) * _config.roi.right);
    int roiPixelTop = qRound(double(height()) * _config.roi.top);
    int roiPixelBottom = qRound(double(height()) * _config.roi.bottom);
    bool roiOn = _config.roiMode == ROI_SINGLE;
    bool oldRoiOn = roiOn;
    opts.items = {
        new ConfigItemBool(cfgPlot, qApp->tr("Normalize data"), &_config.plot.normalize),
        new ConfigItemSpace(cfgPlot, 12),
        (new ConfigItemBool(cfgPlot, qApp->tr("Colorize over full range"), &scaleFullRange))
            ->withRadioGroup("colorize"),
        (new ConfigItemBool(cfgPlot, qApp->tr("Colorize over data range"), &scaleDataRange))
            ->withRadioGroup("colorize"),
        new ConfigItemSpace(cfgPlot, 12),
        new ConfigItemBool(cfgPlot, qApp->tr("Rescale pixels"), &rescalePlot),
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
            ->withAlignment(Qt::AlignRight)
    };
    if (canMavg()) {
        opts.items
            << new ConfigItemBool(cfgTable, qApp->tr("Show moving average"), &_config.mavg.on)
            << (new ConfigItemInt(cfgTable, qApp->tr("Over number of frames"), &_config.mavg.frames))
                ->withMinMax(2, 100)
        ;
    }
    opts.items
        << new ConfigItemBool(cfgBgnd, qApp->tr("Subtract background"), &_config.bgnd.on)
        << (new ConfigItemReal(cfgBgnd, qApp->tr("Corner Fraction %"), &cornerFraction))
            ->withHint(qApp->tr("ISO 11146 recommends 2-5%"), false)
        << (new ConfigItemReal(cfgBgnd, qApp->tr("Noise Factor"), &_config.bgnd.noise))
            ->withHint(qApp->tr("ISO 11146 recommends 2-4"), false)

        << (new ConfigItemReal(cfgCentr, qApp->tr("Mask Diameter"), &_config.bgnd.mask))
            ->withHint(qApp->tr("ISO 11146 recommends 3"), false)
        << (new ConfigItemInt(cfgCentr, qApp->tr("Max Iterations"), &_config.bgnd.iters))
            ->withMinMax(0, 50)
        << new ConfigItemReal(cfgCentr, qApp->tr("Precision"), &_config.bgnd.precision)

        << (new ConfigItemBool(cfgRoi, qApp->tr("Use region"), &roiOn))
            ->withHint(qApp->tr(
                "These are raw pixels values. "
                "Use the menu command <b>Camera ► Edit ROI</b> "
                "to change region interactively in scaled units"), true)
        << (new ConfigItemInt(cfgRoi, qApp->tr("Left"), &roiPixelLeft))
            ->withMinMax(0, width())
        << (new ConfigItemInt(cfgRoi, qApp->tr("Top"), &roiPixelTop))
            ->withMinMax(0, height())
        << (new ConfigItemInt(cfgRoi, qApp->tr("Right"), &roiPixelRight))
            ->withMinMax(0, width())
        << (new ConfigItemInt(cfgRoi, qApp->tr("Bottom"), &roiPixelBottom))
            ->withMinMax(0, height())
    ;
    initConfigMore(opts);
    if (ConfigDlg::edit(opts))
    {
        RoiRect oldRoi = _config.roi;
        _config.plot.fullRange = scaleFullRange;
        _config.plot.rescale = rescalePlot;
        _config.plot.customScale.on = useCustomScale;
        _config.bgnd.corner = cornerFraction / 100.0;
        _config.roi.left = double(roiPixelLeft)/double(width());
        _config.roi.right = double(roiPixelRight)/double(width());
        _config.roi.top = double(roiPixelTop)/double(height());
        _config.roi.bottom = double(roiPixelBottom)/double(height());
        _config.roi.fix();
        if (oldRoiOn != roiOn) {
            _config.roiMode = roiOn ? ROI_SINGLE : ROI_NONE;
        }
        saveConfig(true);
        if (!oldRoi.isEqual(_config.roi))
            raisePowerWarning();
        return true;
    }
    return false;
}

void Camera::setRoi(const RoiRect &roi)
{
    bool powerWarning = !roi.isEqual(_config.roi);
    _config.roi = roi;
    _config.roi.fix();
    saveConfig();
    if (powerWarning)
        raisePowerWarning();
}

void Camera::setRois(const QList<RoiRect>& rois)
{
    bool powerWarning = false;
    if (_config.rois.size() != rois.size())
        powerWarning = true;
    else {
        for (int i = 0; i < rois.size(); i++)
            if (!_config.rois.at(i).isEqual(rois.at(i))) {
                powerWarning = true;
                break;
            }
    }
    _config.rois = rois;
    for (auto roi : std::as_const(_config.rois)) roi.fix();
    saveConfig();
    if (powerWarning)
        raisePowerWarning();
}

void Camera::setRois(const QList<QPointF>& points)
{
    QList<RoiRect> rois;
    for (const auto &p : points) {
        RoiRect roi;
        roi.left = p.x() - _config.mroiSize.w / 2.0;
        roi.top = p.y() - _config.mroiSize.h / 2.0;
        roi.right = p.x() + _config.mroiSize.w / 2.0;
        roi.bottom = p.y() + _config.mroiSize.h / 2.0;
        rois << roi;
    }
    setRois(rois);
}

void Camera::setRoisSize(const FrameSize& sz)
{
    _config.mroiSize = sz;

    QList<QPointF> points;
    for (const auto &roi : std::as_const(_config.rois)) {
        points << QPointF(
            (roi.left + roi.right) / 2.0,
            (roi.top + roi.bottom) / 2.0
        );
    }
    setRois(points);
}

void Camera::setRoiMode(RoiMode mode)
{
    if (_config.roiMode != mode) {
        _config.roiMode = mode;
        saveConfig();
        raisePowerWarning();
    }
}

void Camera::setColorMap(const QString& colorMap)
{
    if (_config.plot.colorMap != colorMap) {
        _config.plot.colorMap = colorMap;
        saveConfig();
    }
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
        .withOnHelp([]{ HelpSystem::topic("power_meter"); })
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
    }
    return ok;
}

TableRowsSpec Camera::tableRows() const
{
    TableRowsSpec rows;
    rows.showSdev = _config.mavg.on;
    if (_config.roiMode == ROI_NONE || _config.roiMode == ROI_SINGLE) {
        rows.results << qApp->tr("Centroid");
    } else {
        for (int i = 0; i < _config.rois.size(); i++) {
            const auto &roi = _config.rois.at(i);
            if (roi.label.isEmpty())
                rows.results << qApp->tr("Result #%1").arg(i+1);
            else rows.results << roi.label;
        }
    }
    return rows;
}

bool Camera::editRoisSize()
{
    auto scale = pixelScale();
    double imgW = scale.pixelToUnit(width());
    double imgH = scale.pixelToUnit(height());
    auto edW = Ori::Gui::spinBox(10, imgW, qRound(imgW * _config.mroiSize.w));
    auto edH = Ori::Gui::spinBox(10, imgH, qRound(imgH * _config.mroiSize.h));
    edW->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    edH->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QString suffix = scale.on ? scale.unit : QStringLiteral("px");
    auto w = LayoutV({
        qApp->tr("Several ROIs are building around\n"
            "crosshairs, they have the same size:"),
        SpaceV(),
        qApp->tr("Width"), LayoutH({ edW, suffix }),
        SpaceV(),
        qApp->tr("Height"), LayoutH({ edH, suffix }),
    }).setMargin(0).makeWidgetAuto();
    bool ok = Dialog(w)
        .withContentToButtonsSpacingFactor(3)
        .windowModal()
        .exec();
    if (ok) {
        setRoisSize({
            double(edW->value()) / imgW,
            double(edH->value()) / imgH
        });
    }
    return ok;
}
