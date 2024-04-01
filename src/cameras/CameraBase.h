#ifndef CAMERA_BASE_H
#define CAMERA_BASE_H

#include <QString>

struct CameraInfo
{
    QString name;
    QString descr;
    int width;
    int height;
    int bits;

    QString resolutionStr() const;
};

#endif // CAMERA_BASE_H