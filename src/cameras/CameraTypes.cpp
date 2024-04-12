#include "CameraTypes.h"

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
