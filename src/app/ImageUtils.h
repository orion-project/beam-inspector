#ifndef IMAGE_UTILS_H
#define IMAGE_UTILS_H

#include <QString>
#include <QByteArray>

namespace ImageUtils
{

struct PgmData {
    QByteArray data;
    int width = 0;
    int height = 0;
    int bpp = 0;
    QString error;
    
    bool isValid() const { return error.isEmpty() && !data.isEmpty(); }
};

QString savePgm(const QString &fileName, const QByteArray &data, int width, int height, int bpp);
PgmData loadPgm(const QString &fileName);

}

#endif // IMAGE_UTILS_H
