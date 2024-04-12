#include "Camera.h"

#include "helpers/OriDialogs.h"
#include "helpers/OriLayouts.h"
#include "helpers/OriWidgets.h"
#include "tools/OriSettings.h"
#include "widgets/OriValueEdit.h"

#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QSpinBox>
#include <QStyleHints>

using namespace Ori::Layouts;

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
    auto normalize = new QCheckBox(qApp->tr("Normalize data"));
    auto subtractBackground = new QCheckBox(qApp->tr("Subtract background"));
    auto maxIters = Ori::Gui::spinBox(0, 50);
    auto precision = new Ori::Widgets::ValueEdit;
    auto cornerFraction = new Ori::Widgets::ValueEdit;
    auto nT = new Ori::Widgets::ValueEdit;
    auto maskDiam = new Ori::Widgets::ValueEdit;

    normalize->setChecked(_config.normalize);
    subtractBackground->setChecked(_config.subtractBackground);
    maxIters->setValue(_config.maxIters);
    precision->setValue(_config.precision);
    cornerFraction->setValue(_config.cornerFraction * 100);
    nT->setValue(_config.nT);
    maskDiam->setValue(_config.maskDiam);

    auto hintLabel = [](const QString& text){
        auto label = new QLabel(text);
        label->setForegroundRole(qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark ? QPalette::Light : QPalette::Mid);
        label->setWordWrap(true);
        return label;
    };

    auto w = LayoutV({
        normalize,
        subtractBackground, Space(12),
        qApp->tr("Max Iterations:"), maxIters, Space(12),
        qApp->tr("Precision:"), precision, Space(12),

        qApp->tr("Corner Fraction %:"),
        cornerFraction,
        hintLabel(qApp->tr("ISO 11146 recommends 2-5%")),
        Space(12),

        qApp->tr("Noise Factor:"),
        nT,
        hintLabel(qApp->tr("ISO 11146 recommends 2-4")),
        Space(12),

        qApp->tr("Mask Diameter:"),
        maskDiam,
        hintLabel(qApp->tr("ISO 11146 recommends 3")),
        Space(12),

    }).setSpacing(3).makeWidgetAuto();

    bool ok = Ori::Dlg::Dialog(w).withPersistenceId("cam_config").exec();
    if (ok) {
        _config.normalize = normalize->isChecked();
        _config.subtractBackground = subtractBackground->isChecked();
        _config.maxIters = maxIters->value();
        _config.precision = precision->value();
        _config.cornerFraction = cornerFraction->value() / 100.0;
        _config.nT = nT->value();
        _config.maskDiam = maskDiam->value();
        saveConfig();
    }
    return ok;
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
