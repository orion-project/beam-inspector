#include "CameraTypes.h"

#include "core/OriFloatingPoint.h"

#include <QSettings>

QString formatSecs(int secs) {
    int h = secs / 3600;
    int m = (secs - h * 3600) / 60;
    int s = secs - h * 3600 - m * 60;
    QStringList strs;
    if (h > 0) strs << QStringLiteral("%1h").arg(h);
    if (m > 0) strs << QStringLiteral("%1m").arg(m);
    if (s > 0) strs << QStringLiteral("%1s").arg(s);
    return strs.join(' ');
}

//------------------------------------------------------------------------------
//                               CameraConfig
//------------------------------------------------------------------------------

#define LOAD(option, type, def) option = s->value(QStringLiteral(#option), def).to ## type()
#define SAVE(option) s->setValue(QStringLiteral(#option), option)

void CameraConfig::load(QSettings *s)
{
    LOAD(plot.normalize, Bool, true);
    LOAD(plot.rescale, Bool, false);
    LOAD(plot.fullRange, Bool, true);
    LOAD(plot.customScale.on, Bool, true);
    LOAD(plot.customScale.factor, Double, 5);
    LOAD(plot.customScale.unit, String, "um");

    LOAD(bgnd.on, Bool, true);
    LOAD(bgnd.iters, Int, 0);
    LOAD(bgnd.precision, Double, 0.05);
    LOAD(bgnd.corner, Double, 0.035);
    LOAD(bgnd.noise, Double, 3);
    LOAD(bgnd.mask, Double, 3);

    LOAD(roi.on, Bool, false);
    LOAD(roi.left, Double, 0.25);
    LOAD(roi.top, Double, 0.25);
    LOAD(roi.right, Double, 0.75);
    LOAD(roi.bottom, Double, 0.75);

    LOAD(power.on, Bool, false);
    LOAD(power.avgFrames, Int, 4);
    LOAD(power.power, Double, 0.0);
}

void CameraConfig::save(QSettings *s, bool compact) const
{
    SAVE(plot.normalize);
    SAVE(plot.rescale);
    SAVE(plot.fullRange);
    SAVE(plot.customScale.on);
    if (!compact or plot.customScale.on) {
        SAVE(plot.customScale.factor);
        SAVE(plot.customScale.unit);
    }

    SAVE(bgnd.on);
    if (!compact or bgnd.on) {
        SAVE(bgnd.iters);
        SAVE(bgnd.precision);
        SAVE(bgnd.corner);
        SAVE(bgnd.noise);
        SAVE(bgnd.mask);
    }

    SAVE(roi.on);
    if (!compact or roi.on) {
        SAVE(roi.left);
        SAVE(roi.top);
        SAVE(roi.right);
        SAVE(roi.bottom);
    }

    SAVE(power.on);
    if (!compact or power.on) {
        SAVE(power.avgFrames);
        SAVE(power.power);
    }
}

//------------------------------------------------------------------------------
//                               PixelScale
//------------------------------------------------------------------------------

QString PixelScale::format(const double &v) const
{
    if (on)
        return QStringLiteral("%1 %2").arg(v*factor, 0, 'f', 1).arg(unit);
    return QStringLiteral("%1").arg(v, 0, 'f', 1);
}

QString PixelScale::formatWithMargins(const double &v) const
{
    if (on)
        return QStringLiteral(" %1 %2 ").arg(v*factor, 0, 'f', 1).arg(unit);
    return QStringLiteral(" %1 ").arg(v, 0, 'f', 1);
}

bool PixelScale::operator ==(const PixelScale& s) const {
    return
        on == s.on &&
        int(factor * 1000) == int(s.factor * 1000) &&
        unit == s.unit;
}

//------------------------------------------------------------------------------
//                               RoiRect
//------------------------------------------------------------------------------

bool RoiRect::isValid() const
{
    return left >= 0 && right > left && right <= 1 && top >= 0 && bottom > top && top <= 1;
}

bool RoiRect::isZero() const
{
    return Double(left).isZero() && Double(top).isZero() && Double(right).isZero() && Double(bottom).isZero();
}

QString RoiRect::sizeStr(int w, int h) const
{
    return QStringLiteral("%1 × %2")
        .arg(qRound(double(w)*(right-left)))
        .arg(qRound(double(h)*(bottom-top)));
}

void RoiRect::fix()
{
    if (left > right) std::swap(left, right);
    if (top > bottom) std::swap(top, bottom);
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > 1) right = 1;
    if (bottom > 1) bottom = 1;
    if (Double(left).is(right)) {
        if (Double(right).is(1)) left = 0;
        else right = 1;
    }
    if (Double(top).is(bottom)) {
        if (Double(bottom).is(1)) top = 0;
        else bottom = 1;
    }
}
