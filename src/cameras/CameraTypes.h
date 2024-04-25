#ifndef CAMERA_TYPES_H
#define CAMERA_TYPES_H

#include <QDateTime>
#include <QEvent>
#include <QString>

class QSettings;

struct CameraItem
{
    QVariant id;
    QString name;
};

struct RoiRect
{
    bool on = false;
    int x1 = 0;
    int y1 = 0;
    int x2 = 0;
    int y2 = 0;

    int width() const { return x2 - x1; }
    int height() const { return y2 - y1; }
    bool isValid(int w, int h) const;
    bool isZero() const;
    void fix(int w, int h);
    QString sizeStr() const;
};

struct PixelScale
{
    bool on = false;
    double factor = 1;
    QString unit = "um";

    inline double sensorToUnit(const double& v) const { return on ? v*factor : v; }
    inline double unitToSensor(const double& v) const { return on ? v/factor : v; }

    bool operator ==(const PixelScale& s) const;
    bool operator !=(const PixelScale& s) const { return !(*this == s); }

    QString format(const double &v) const;
    QString formatWithMargins(const double &v) const;
};

struct Background
{
    bool on = true;
    int iters = 0;
    double precision = 0.05;
    double corner = 0.035;
    double noise = 3;
    double mask = 3;
};

struct PlotOptions
{
    bool normalize = true;
    bool rescale = false;
    PixelScale customScale;
};

struct CameraConfig
{
    PlotOptions plot;
    Background bgnd;
    RoiRect roi;

    void load(QSettings *s);
    void save(QSettings *s, bool min=false) const;
};

struct Measurement
{
    qint64 time;
    bool nan;
    double xc;
    double yc;
    double dx;
    double dy;
    double phi;
    inline double eps() const { return qMin(dx, dy) / qMax(dx, dy); }
};

class MeasureEvent : public QEvent
{
public:
    MeasureEvent() : QEvent(QEvent::User) {}

    int num;
    int count;
    Measurement *results;
    QDateTime start;
};

struct CameraStats
{
    int fps;
    qint64 measureTime;
    QString errorFrames;
};

#endif // CAMERA_TYPES_H
