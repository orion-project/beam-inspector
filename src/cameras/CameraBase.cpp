#include "CameraBase.h"


QString CameraInfo::resolutionStr() const
{
    return QStringLiteral("%1 × %2 × %3bit").arg(width).arg(height).arg(bits);
}
