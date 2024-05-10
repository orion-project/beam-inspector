#include "CameraTypes.h"

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
    LOAD(roi.x1, Int, 0);
    LOAD(roi.y1, Int, 0);
    LOAD(roi.x2, Int, 0);
    LOAD(roi.y2, Int, 0);
}

void CameraConfig::save(QSettings *s, bool min) const
{
    SAVE(plot.normalize);
    SAVE(plot.rescale);
    SAVE(plot.customScale.on);
    if (!min or plot.customScale.on) {
        SAVE(plot.customScale.factor);
        SAVE(plot.customScale.unit);
    }

    SAVE(bgnd.on);
    if (!min or bgnd.on) {
        SAVE(bgnd.iters);
        SAVE(bgnd.precision);
        SAVE(bgnd.corner);
        SAVE(bgnd.noise);
        SAVE(bgnd.mask);
    }

    SAVE(roi.on);
    if (!min or roi.on) {
        SAVE(roi.x1);
        SAVE(roi.y1);
        SAVE(roi.x2);
        SAVE(roi.y2);
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

bool RoiRect::isValid(int w, int h) const
{
    return x1 >= 0 && x2 > x1 && x2 <= w && y1 >= 0 && y2 > y1 && y2 <= h;
}

bool RoiRect::isZero() const
{
    return x1 == 0 && y1 == 0 && x2 == 0 && y2 == 0;
}

QString RoiRect::sizeStr() const
{
    return QStringLiteral("%1 × %2").arg(x2-x1).arg(y2-y1);
}

void RoiRect::fix(int w, int h)
{
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > w) x2 = w;
    if (y2 > h) y2 = h;
    if (x1 == x2) {
        if (x2 == w) x1 = 0;
        else x2 = w;
    }
    if (y1 == y2) {
        if (y2 == h) y1 = 0;
        else y2 = h;
    }
}
