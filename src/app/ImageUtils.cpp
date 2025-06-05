#include "ImageUtils.h"

#include <QFile>
#include <QTextStream>

namespace ImageUtils {

QString savePgm(const QString &fileName, const QByteArray &data, int width, int height, int bpp)
{
    QFile f(fileName);
    if (!f.open(QIODevice::WriteOnly)) {
        return f.errorString();
    }
    {
        QTextStream header(&f);
        header << "P5\n" << width << ' ' << height << '\n' << (1<<bpp)-1 << '\n';
    }
    if (bpp > 8) {
        auto buf = (uint16_t*)data.data();
        for (int i = 0; i < width*height; i++) {
            uint16_t pixel = (buf[i] >> 8) | (buf[i] << 8);
            f.write((const char*)(&pixel), sizeof(uint16_t));
        }
    } else {
        f.write(data);
    }
    return QString();
}

} // namespace ImageUtils
