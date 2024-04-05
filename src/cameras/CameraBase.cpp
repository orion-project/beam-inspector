#include "CameraBase.h"

#include <QDebug>

QString CameraInfo::resolutionStr() const
{
    return QStringLiteral("%1 × %2 × %3bit").arg(width).arg(height).arg(bits);
}

void CameraSettings::print()
{
    qDebug() << "subtractBackground:" << subtractBackground;
}
