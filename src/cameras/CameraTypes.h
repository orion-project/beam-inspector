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
    double left = 0;
    double top = 0;
    double right = 0;
    double bottom = 0;
    QString label;

    int width() const { return right - left; }
    int height() const { return bottom - top; }
    bool isValid() const;
    bool isZero() const;
    bool isEqual(const RoiRect &r) const {
        return left == r.left && top == r.top && right == r.right && bottom == r.bottom; }
    void fix();
    QString sizeStr(int w, int h) const;
};

using RoiRects = QList<RoiRect>;

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
    QString colorMap;
};

struct PowerMeter
{
    bool on = false;
    int avgFrames = 4;
    double power = 0.0;
    int decimalFactor = 0;

    static int minAvgFrames;
    static int maxAvgFrames;
};

enum RoiMode { ROI_NONE, ROI_SINGLE, ROI_MULTI };

struct FrameSize
{
    double w;
    double h;
};

struct Averaging
{
    bool on = false;
    int frames = 5;
};

struct CameraConfig
{
    PlotOptions plot;
    Background bgnd;
    RoiRect roi;
    RoiRects rois;
    RoiMode roiMode = ROI_NONE;
    FrameSize mroiSize = { 0.1, 0.1 };
    PowerMeter power;
    Averaging mavg;

    void load(QSettings *s);
    void save(QSettings *s, bool compact=false) const;
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
    enum { TEXT, MS, COUNT, POWER, VALUE3 } type = MS;
    bool warn = false;
};

class BrightEvent : public QEvent
{
public:
    BrightEvent() : QEvent(QEvent::Type(QEvent::registerEventType())) {}

    double level;
};

class ExpWarningEvent : public QEvent
{
public:
    ExpWarningEvent() : QEvent(QEvent::Type(QEvent::registerEventType())) {}

    double overexposed;
};

class UpdateSettingsEvent : public QEvent
{
public:
    UpdateSettingsEvent() : QEvent(QEvent::Type(QEvent::registerEventType())) {}
};

struct RandomOffset
{
    RandomOffset() {}
    RandomOffset(double min, double max);
    RandomOffset(double start, double min, double max);

    double rnd() const;
    double next();
    double value() const { return c; }

    double c, v, dv, v_min, v_max, h, rnd_max;
};

struct CameraCommons
{
    static QString supportedImageFilters();
    static const QStringList& supportedImageExts();
};

struct TableRowsSpec
{
    bool showSdev = false;
    QStringList results;
    QList<QPair<int, QString>> aux;
};

struct GoodnessLimits
{
    double deltaMin = 0.005;
    double deltaMax = 0.05;
};

using AnyRecord = QMap<QString, QVariant>;
using AnyRecords = QList<AnyRecord>;

#define HEX(v) QStringLiteral("0x%1").arg(v, 0, 16)

#endif // CAMERA_TYPES_H
