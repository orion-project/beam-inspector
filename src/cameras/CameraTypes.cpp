#include "CameraTypes.h"

//------------------------------------------------------------------------------
//                               PixelScale
//------------------------------------------------------------------------------

QString PixelScale::format(const double &v) const
{
    if (on)
        return QStringLiteral("%1 %2").arg(v*factor, 0, 'f', 0).arg(unit);
    return QStringLiteral("%1").arg(v, 0, 'f', 0);
}

QString PixelScale::formatWithMargins(const double &v) const
{
    if (on)
        return QStringLiteral(" %1 %2 ").arg(v*factor, 0, 'f', 0).arg(unit);
    return QStringLiteral(" %1 ").arg(v, 0, 'f', 0);
}

bool PixelScale::operator ==(const PixelScale& s) const {
    return
        on == s.on &&
        int(factor * 1000) == int(s.factor * 1000) &&
        unit == s.unit;
}

//------------------------------------------------------------------------------
//                               SoftAperture
//------------------------------------------------------------------------------

bool SoftAperture::isValid(int w, int h) const
{
    return x1 >= 0 && x2 > x1 && x2 <= w && y1 >= 0 && y2 > y1 && y2 <= h;
}

bool SoftAperture::isZero() const
{
    return x1 == 0 && y1 == 0 && x2 == 0 && y2 == 0;
}

QString SoftAperture::sizeStr() const
{
    return QStringLiteral("%1 × %2").arg(x2-x1).arg(y2-y1);
}

void SoftAperture::fix(int w, int h)
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
