#ifndef CAMERA_TYPES_H
#define CAMERA_TYPES_H

#include <QEvent>
#include <QVariant>

class QSettings;

QString formatSecs(int secs);

struct CameraItem
{
    /// Some internal id known by the camera framework using to open a camera.
    QVariant cameraId;

    /// An id used to get and set a custom name for a camera, should be unique over the system.
    QString customId;

    /// Default display name to be shown in the camera selector.
    QString displayName;
};

struct RoiRect
{
    bool on = false;
    double left = 0;
    double top = 0;
    double right = 0;
    double bottom = 0;

    int width() const { return right - left; }
    int height() const { return bottom - top; }
    bool isValid() const;
    bool isZero() const;
    void fix();
    QString sizeStr(int w, int h) const;
};

struct PixelScale
{
    bool on = false;
    double factor = 1;
    QString unit = "um";

    inline double pixelToUnit(const double& v) const { return on ? v*factor : v; }
    inline double unitToPixel(const double& v) const { return on ? v/factor : v; }

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
    bool fullRange = true;
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

struct CameraStats
{
    double fps;
    double hardFps = 0;
    qint64 measureTime;
};

struct CamTableData
{
    QVariant value;
    enum { NONE, MS, COUNT, VALUE3 } type = MS;
    bool warn = false;
};

class BrightEvent : public QEvent
{
public:
    BrightEvent() : QEvent(QEvent::User) {}

    double level;
};

#endif // CAMERA_TYPES_H
