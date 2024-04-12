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

#include <QDebug>

//------------------------------------------------------------------------------
//                                CameraInfo
//------------------------------------------------------------------------------

QString CameraInfo::resolutionStr() const
{
    return QStringLiteral("%1 × %2 × %3bit").arg(width).arg(height).arg(bits);
}

//------------------------------------------------------------------------------
//                               SoftAperture
//------------------------------------------------------------------------------

bool SoftAperture::isValid(int w, int h) const
{
    return x1 >= 0 && x2 > x1 && x2 <= w && y1 >= 0 && y2 > y1 && y2 <= h;
}

QString SoftAperture::sizeStr() const
{
    return QStringLiteral("%1 × %2").arg(x2-x1).arg(y2-y1);
}

//------------------------------------------------------------------------------
//                                CameraSettings
//------------------------------------------------------------------------------

void CameraSettings::print()
{
    qDebug() << "subtractBackground:" << subtractBackground << "maxIters:" << maxIters
        << "precision:" << precision << "cornerFraction:" << cornerFraction << "nT:" << nT
        << "maskDiam:" << maskDiam;
}

void CameraSettings::load(const QString &group)
{
    this->group = group;

    Ori::Settings s;
    s.beginGroup(group);

    normalize = s.value("normalize", true).toBool();
    subtractBackground = s.value("subtractBackground", true).toBool();

    maxIters = s.value("maxIters", 0).toInt();
    if (maxIters <= 0) maxIters = 0;

    precision = s.value("precision", 0.05).toDouble();
    if (precision <= 0) precision = 0.05;

    cornerFraction = s.value("cornerFraction", 0.035).toDouble();
    if (cornerFraction <= 0) cornerFraction = 0.035;

    nT = s.value("nT", 3).toDouble();
    if (nT <= 0) nT = 3;

    maskDiam = s.value("maskDiam", 3).toDouble();
    if (maskDiam <= 0) maskDiam = 3;

    aperture.enabled = s.value("aperture/on", false).toBool();
    aperture.x1 = s.value("aperture/x1", 0).toInt();
    aperture.y1 = s.value("aperture/y1", 0).toInt();
    aperture.x2 = s.value("aperture/x2", 0).toInt();
    aperture.y2 = s.value("aperture/y2", 0).toInt();
}

void CameraSettings::save(const QString &group)
{
    if (group.isEmpty() && this->group.isEmpty()) {
        qWarning() << "Unable to save camera settings, groups is not set";
        return;
    }

    Ori::Settings s;
    s.beginGroup(group.isEmpty() ? this->group : group);
    s.setValue("normalize", normalize);
    s.setValue("subtractBackground", subtractBackground);
    s.setValue("maxIters", maxIters);
    s.setValue("precision", precision);
    s.setValue("cornerFraction", cornerFraction);
    s.setValue("nT", nT);
    s.setValue("maskDiam", maskDiam);
    s.setValue("aperture/on", aperture.enabled);
    s.setValue("aperture/x1", aperture.x1);
    s.setValue("aperture/y1", aperture.y1);
    s.setValue("aperture/x2", aperture.x2);
    s.setValue("aperture/y2", aperture.y2);
}

bool CameraSettings::editDlg(const QString &group)
{
    CameraSettings s;
    s.load(group);

    auto normalize = new QCheckBox(qApp->tr("Normalize data"));
    auto subtractBackground = new QCheckBox(qApp->tr("Subtract background"));
    auto maxIters = Ori::Gui::spinBox(0, 50);
    auto precision = new Ori::Widgets::ValueEdit;
    auto cornerFraction = new Ori::Widgets::ValueEdit;
    auto nT = new Ori::Widgets::ValueEdit;
    auto maskDiam = new Ori::Widgets::ValueEdit;

    normalize->setChecked(s.normalize);
    subtractBackground->setChecked(s.subtractBackground);
    maxIters->setValue(s.maxIters);
    precision->setValue(s.precision);
    cornerFraction->setValue(s.cornerFraction * 100);
    nT->setValue(s.nT);
    maskDiam->setValue(s.maskDiam);

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

    bool ok = Ori::Dlg::Dialog(w).withPersistenceId(group).exec();

    if (ok) {
        s.normalize = normalize->isChecked();
        s.subtractBackground = subtractBackground->isChecked();
        s.maxIters = maxIters->value();
        s.precision = precision->value();
        s.cornerFraction = cornerFraction->value() / 100.0;
        s.nT = nT->value();
        s.maskDiam = maskDiam->value();
        s.save();
    }

    return ok;
}
