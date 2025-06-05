#ifndef IMAGE_UTILS_H
#define IMAGE_UTILS_H

#include <QString>

namespace ImageUtils
{

QString savePgm(const QString &fileName, const QByteArray &data, int width, int height, int bpp);

}

#endif // IMAGE_UTILS_H
