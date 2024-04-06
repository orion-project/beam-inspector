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

struct CameraSettings
{
    bool subtractBackground = true;
    int maxIters = 0;
    double precision = 0.05;
    double cornerFraction = 0.035;
    double nT = 3;
    double maskDiam = 3;

    void print();
    void load(const QString &group);
    void save(const QString &group);

    static bool editDlg(const QString &group);
};

#endif // CAMERA_BASE_H
