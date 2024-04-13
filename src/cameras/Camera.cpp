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

void Camera::loadConfig()
{
    Ori::Settings s;
    s.beginGroup(_configGroup);
    _config.normalize = s.value("normalize", true).toBool();
    _config.subtractBackground = s.value("subtractBackground", true).toBool();
    _config.maxIters = s.value("maxIters", 0).toInt();
    _config.precision = s.value("precision", 0.05).toDouble();
    _config.cornerFraction = s.value("cornerFraction", 0.035).toDouble();
    _config.nT = s.value("nT", 3).toDouble();
    _config.maskDiam = s.value("maskDiam", 3).toDouble();
    _config.aperture.enabled = s.value("aperture/on", false).toBool();
    _config.aperture.x1 = s.value("aperture/x1", 0).toInt();
    _config.aperture.y1 = s.value("aperture/y1", 0).toInt();
    _config.aperture.x2 = s.value("aperture/x2", 0).toInt();
    _config.aperture.y2 = s.value("aperture/y2", 0).toInt();
}

void Camera::saveConfig()
{
    Ori::Settings s;
    s.beginGroup(_configGroup);
    s.setValue("normalize", _config.normalize);
    s.setValue("subtractBackground", _config.subtractBackground);
    s.setValue("maxIters", _config.maxIters);
    s.setValue("precision", _config.precision);
    s.setValue("cornerFraction", _config.cornerFraction);
    s.setValue("nT", _config.nT);
    s.setValue("maskDiam", _config.maskDiam);
    s.setValue("aperture/on", _config.aperture.enabled);
    s.setValue("aperture/x1", _config.aperture.x1);
    s.setValue("aperture/y1", _config.aperture.y1);
    s.setValue("aperture/x2", _config.aperture.x2);
    s.setValue("aperture/y2", _config.aperture.y2);
}

bool Camera::editConfig()
{
    ConfigDlgOpts opts;
    opts.objectName = "CamConfigDlg";
    opts.pageIconSize = 32;
    opts.pages = {
        ConfigPage(cfgPlot, qApp->tr("Plot"), ":/toolbar/beam"),
        ConfigPage(cfgCalc, qApp->tr("Processing"), ":/toolbar/settings").withSpacing(12),
        ConfigPage(cfgAper, qApp->tr("Aperture"), ":/toolbar/aperture").withSpacing(12),
    };
    opts.items = {
        new ConfigItemBool(cfgPlot, qApp->tr("Normalize data"), &_config.normalize),

        new ConfigItemBool(cfgCalc, qApp->tr("Subtract background"), &_config.subtractBackground),
        (new ConfigItemInt(cfgCalc, qApp->tr("Max iterations"), &_config.maxIters))
            ->withMinMax(0, 50),
        new ConfigItemReal(cfgCalc, qApp->tr("Precision"), &_config.precision),
        (new ConfigItemReal(cfgCalc, qApp->tr("Corner Fraction %"), &_config.cornerFraction))
            ->withHint(qApp->tr("ISO 11146 recommends 2-5%"), false),
        (new ConfigItemReal(cfgCalc, qApp->tr("Noise Factor"), &_config.nT))
            ->withHint(qApp->tr("ISO 11146 recommends 2-4"), false),
        (new ConfigItemReal(cfgCalc, qApp->tr("Mask Diameter"), &_config.maskDiam))
            ->withHint(qApp->tr("ISO 11146 recommends 3"), false),

        new ConfigItemBool(cfgAper, qApp->tr("Use soft aperture"), &_config.aperture.enabled),
        (new ConfigItemInt(cfgAper, qApp->tr("Left"), &_config.aperture.x1))
            ->withMinMax(0, width()),
        (new ConfigItemInt(cfgAper, qApp->tr("Top"), &_config.aperture.y1))
            ->withMinMax(0, height()),
        (new ConfigItemInt(cfgAper, qApp->tr("Right"), &_config.aperture.x2))
            ->withMinMax(0, width()),
        (new ConfigItemInt(cfgAper, qApp->tr("Bottom"), &_config.aperture.y2))
            ->withMinMax(0, height())
            ->withHint(qApp->tr(
                "<p>● Values are in pixels"
                "<p>● Use command <b>Camera ► Edit Soft Aperture</b> "
                "to change the aperture interactively"), true),
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
    saveConfig();
}

void Camera::toggleAperture(bool on)
{
    _config.aperture.enabled = on;
    saveConfig();
}
