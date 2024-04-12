#ifndef CAMERA_TYPES_H
#define CAMERA_TYPES_H

#include <QString>

struct SoftAperture
{
    bool enabled = false;
    int x1 = 0;
    int y1 = 0;
    int x2 = 0;
    int y2 = 0;

    int width() const { return x2 - x1; }
    int height() const { return y2 - y1; }
    bool isValid(int w, int h) const;
    QString sizeStr() const;
};

struct CameraConfig
{
    bool normalize = true;
    bool subtractBackground = true;
    int maxIters = 0;
    double precision = 0.05;
    double cornerFraction = 0.035;
    double nT = 3;
    double maskDiam = 3;
    SoftAperture aperture;
};

#endif // CAMERA_TYPES_H
