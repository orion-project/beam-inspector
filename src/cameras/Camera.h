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

struct SoftAperture
{
    bool enabled = false;
    int x1 = 0;
    int y1 = 0;
    int x2 = 0;
    int y2 = 0;

    bool isValid(int w, int h) const;
    QString sizeStr() const;
};

struct CameraSettings
{
    QString group;
    bool normalize = true;
    bool subtractBackground = true;
    int maxIters = 0;
    double precision = 0.05;
    double cornerFraction = 0.035;
    double nT = 3;
    double maskDiam = 3;
    SoftAperture aperture;

    void print();
    void load(const QString &group);
    void save(const QString &group = QString());

    static bool editDlg(const QString &group);
};

#endif // CAMERA_BASE_H
