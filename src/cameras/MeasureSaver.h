#ifndef MEASURE_SAVER_H
#define MEASURE_SAVER_H

#include <QDateTime>
#include <QEvent>
#include <QMap>
#include <QObject>
#include <QSharedPointer>

class QSettings;

class Camera;
struct CsvFile;

#define MULTIRES_IDX 100
#define MULTIRES_IDX_NAN(i) (MULTIRES_IDX*(i+1) + 0)
#define MULTIRES_IDX_DX(i)  (MULTIRES_IDX*(i+1) + 1)
#define MULTIRES_IDX_DY(i)  (MULTIRES_IDX*(i+1) + 2)
#define MULTIRES_IDX_XC(i)  (MULTIRES_IDX*(i+1) + 3)
#define MULTIRES_IDX_YC(i)  (MULTIRES_IDX*(i+1) + 4)
#define MULTIRES_IDX_PHI(i) (MULTIRES_IDX*(i+1) + 5)

struct MeasureConfig
{
    QString fileName;
    bool allFrames;
    int intervalSecs;
    bool average;
    bool durationInf;
    QString duration;
    bool saveImg;
    QString imgInterval;

    void load(QSettings *s);
    void save(QSettings *s, bool min=false) const;
    void loadRecent();
    void saveRecent() const;
    int durationSecs() const;
    int imgIntervalSecs() const;
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
    QMap<int, double> cols;
};

#define EPS(dx, dy) (qMin(dx, dy) / qMax(dx, dy))

class MeasureEvent : public QEvent
{
public:
    MeasureEvent() : QEvent(QEvent::User) {}

    int num;
    int count;
    Measurement *results;
    QMap<QString, QVariant> stats;
};

class ImageEvent : public QEvent
{
public:
    ImageEvent() : QEvent(QEvent::User) {}

    qint64 time;
    QByteArray buf;
};

class MeasureSaver : public QObject
{
    Q_OBJECT

public:
    static std::optional<MeasureConfig> configure();

    MeasureSaver();
    ~MeasureSaver();

    const MeasureConfig& config() const { return _config; }

    QString start(const MeasureConfig &cfg, Camera* cam);

    void setCaptureStart(const QDateTime &t) { _captureStart = t; }

signals:
    void finished();
    void interrupted(const QString &error);

protected:
    bool event(QEvent *event) override;

private:
    QDateTime _captureStart;
    QSharedPointer<QThread> _thread;
    MeasureConfig _config;
    QString _cfgFile, _imgDir;
    QMap<qint64, QString> _errors;
    int _width, _height, _bpp;
    double _scale = 1;
    int _duration = 0;
    qint64 _measureStart;
    qint64 _intervalBeg;
    qint64 _intervalLen;
    int _intervalIdx;
    double _avg_xc, _avg_yc, _avg_dx, _avg_dy, _avg_phi;
    double _avg_cnt;
    QMap<int, double> _multires_avg;
    QMap<int, int> _multires_avg_cnt;
    int _multires_cnt = 0;
    int _savedImgCount = 0;
    QList<int> _auxCols;
    QMap<int, double> _auxAvgVals;
    double _auxAvgCnt;
    std::unique_ptr<CsvFile> _csvFile;

    void processMeasure(MeasureEvent *e);
    void saveImage(ImageEvent *e);

    template <typename T>
    QString formatTime(qint64 time, T fmt) {
        return _captureStart.addMSecs(time).toString(fmt);
    }
};

#endif // MEASURE_SAVER_H
