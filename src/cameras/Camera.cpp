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
    return QStringLiteral("%1 × %2 × %3bit").arg(width()).arg(height()).arg(bits());
}

#define LOAD(option, type, def)\
    _config.option = s.settings()->value(QStringLiteral(#option), def).to ## type()

#define SAVE(option)\
    s.settings()->setValue(QStringLiteral(#option), _config.option)

void Camera::loadConfig()
{
    Ori::Settings s;
    s.beginGroup(_configGroup);

    LOAD(normalize, Bool, true);

    LOAD(bgnd.on, Bool, true);
    LOAD(bgnd.iters, Int, 0);
    LOAD(bgnd.precision, Double, 0.05);
    LOAD(bgnd.corner, Double, 0.035);
    LOAD(bgnd.noise, Double, 3);
    LOAD(bgnd.mask, Double, 3);

    LOAD(aperture.on, Bool, false);
    LOAD(aperture.x1, Int, 0);
    LOAD(aperture.y1, Int, 0);
    LOAD(aperture.x2, Int, 0);
    LOAD(aperture.y2, Int, 0);

    LOAD(scale.on, Bool, false);
    LOAD(scale.factor, Double, 1);
    LOAD(scale.unit, String, "um");
}

void Camera::saveConfig()
{
    Ori::Settings s;
    s.beginGroup(_configGroup);

    SAVE(normalize);

    SAVE(bgnd.on);
    SAVE(bgnd.iters);
    SAVE(bgnd.precision);
    SAVE(bgnd.corner);
    SAVE(bgnd.noise);
    SAVE(bgnd.mask);

    SAVE(aperture.on);
    SAVE(aperture.x1);
    SAVE(aperture.y1);
    SAVE(aperture.x2);
    SAVE(aperture.y2);

    SAVE(scale.on);
    SAVE(scale.factor);
    SAVE(scale.unit);
}

bool Camera::editConfig(ConfigPages page)
{
    ConfigDlgOpts opts;
    opts.currentPageId = page;
    opts.objectName = "CamConfigDlg";
    opts.pageIconSize = 32;
    opts.pages = {
        ConfigPage(cfgPlot, qApp->tr("Plot"), ":/toolbar/zoom_sensor"),
        ConfigPage(cfgBgnd, qApp->tr("Background"), ":/toolbar/beam").withSpacing(12),
        ConfigPage(cfgAper, qApp->tr("Aperture"), ":/toolbar/aper").withSpacing(12),
    };
    opts.items = {
        new ConfigItemBool(cfgPlot, qApp->tr("Normalize data"), &_config.normalize),
        new ConfigItemSpace(cfgPlot, 12),
        new ConfigItemBool(cfgPlot, qApp->tr("Rescale pixels"), &_config.scale.on),
        new ConfigItemReal(cfgPlot, qApp->tr("1px equals to"), &_config.scale.factor),
        (new ConfigItemStr(cfgPlot, qApp->tr("of units"), &_config.scale.unit))
            ->withAlignment(Qt::AlignRight),

        new ConfigItemBool(cfgBgnd, qApp->tr("Subtract background"), &_config.bgnd.on),
        (new ConfigItemInt(cfgBgnd, qApp->tr("Max iterations"), &_config.bgnd.iters))
            ->withMinMax(0, 50),
        new ConfigItemReal(cfgBgnd, qApp->tr("Precision"), &_config.bgnd.precision),
        (new ConfigItemReal(cfgBgnd, qApp->tr("Corner Fraction %"), &_config.bgnd.corner))
            ->withHint(qApp->tr("ISO 11146 recommends 2-5%"), false),
        (new ConfigItemReal(cfgBgnd, qApp->tr("Noise Factor"), &_config.bgnd.noise))
            ->withHint(qApp->tr("ISO 11146 recommends 2-4"), false),
        (new ConfigItemReal(cfgBgnd, qApp->tr("Mask Diameter"), &_config.bgnd.mask))
            ->withHint(qApp->tr("ISO 11146 recommends 3"), false),

        (new ConfigItemBool(cfgAper, qApp->tr("Use soft aperture"), &_config.aperture.on))
            ->withHint(qApp->tr(
                "These are raw pixels values. "
                "Use the menu command <b>Camera ► Edit Soft Aperture</b> "
                "to change aperture interactively in scaled units"), true),
        (new ConfigItemInt(cfgAper, qApp->tr("Left"), &_config.aperture.x1))
            ->withMinMax(0, width()),
        (new ConfigItemInt(cfgAper, qApp->tr("Top"), &_config.aperture.y1))
            ->withMinMax(0, height()),
        (new ConfigItemInt(cfgAper, qApp->tr("Right"), &_config.aperture.x2))
            ->withMinMax(0, width()),
        (new ConfigItemInt(cfgAper, qApp->tr("Bottom"), &_config.aperture.y2))
            ->withMinMax(0, height())
    };
    if (ConfigDlg::edit(opts))
    {
        _config.aperture.fix(width(), height());
        saveConfig();
        return true;
    }
    return false;
}

void Camera::setAperture(const SoftAperture &a)
{
    _config.aperture = a;
    if (_config.aperture.on)
        _config.aperture.fix(width(), height());
    saveConfig();
}

void Camera::toggleAperture(bool on)
{
    _config.aperture.on = on;
    if (on && _config.aperture.isZero()) {
        _config.aperture.x1 = width() / 4;
        _config.aperture.y1 = width() / 4;
        _config.aperture.x2 = width() / 4 * 3;
        _config.aperture.y2 = width() / 4 * 3;
    }
    saveConfig();
}

bool Camera::isApertureValid() const
{
    return _config.aperture.isValid(width(), height());
}
