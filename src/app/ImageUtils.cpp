#include "ImageUtils.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <cmath>

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

PgmData loadPgm(const QString &fileName)
{
    PgmData result;
    
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = file.errorString();
        return result;
    }
    
    std::optional<int> width;
    std::optional<int> height;
    std::optional<int> maxValue;
    qint64 headerEndPos = 0;
    {
        QTextStream header(&file);
        QString magic = header.readLine().trimmed();
        if (magic != "P5") {
            result.error = "Not a valid PGM file (expected P5 format)";
            return result;
        }
        while ((!width || !height || !maxValue) && !header.atEnd()) {
            QString line = header.readLine().trimmed();
            static QRegularExpression whitespace("\\s+") ;
            const QStringList parts = line.split(whitespace, Qt::SkipEmptyParts);
            for (const auto &part : parts) {
                bool isInt = false;
                int value = part.toInt(&isInt);
                if (!isInt) {
                    result.error = "Invalid PGM header format: non-integer value";
                    return result;
                }
                if (!width)
                    width = value;
                else if (!height)
                    height = value;
                else {
                    maxValue = value;
                    headerEndPos = header.pos();
                    break;
                }
            }
        }
    }
    if (!width || !height || !maxValue) {
        result.error = "Invalid PGM header format: missing width/height/maxval";
        return result;
    }
    if (*width <= 0 || *height <= 0) {
        result.error = QString("Invalid image dimensions %1 x %2").arg(*width).arg(*height);
        return result;
    }
    if (*maxValue <= 0 || *maxValue >= 65536) {
        result.error = QString("Invalid max value in PGM header: %1").arg(*maxValue);
        return result;
    }
    // QTextStream somehow scrolls to the end of file
    // so we have to explicitly track current position
    file.seek(headerEndPos);
    result.data = file.readAll();
    result.width = *width;
    result.height = *height;
    result.bpp = std::ceil(std::log2(*maxValue + 1));
    
    int dataSize = result.width * result.height * (result.bpp > 8 ? 2 : 1);
    if (dataSize != result.data.size()) {
        result.data = QByteArray();
        result.error = QString("Invalid data size, expected %1, read %2").arg(dataSize).arg(result.data.size());
        return result;
    }
    
    if (result.bpp > 8) {
        auto buf = reinterpret_cast<uint16_t*>(result.data.data());
        const int numPixels = result.width * result.height;
        for (int i = 0; i < numPixels; i++) {
            buf[i] = (buf[i] >> 8) | (buf[i] << 8);
        }
    }
    
    return result;
}

} // namespace ImageUtils
