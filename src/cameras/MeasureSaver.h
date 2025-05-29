#ifndef MEASURE_SAVER_H
#define MEASURE_SAVER_H

#include <QDateTime>
#include <QEvent>
#include <QMap>
#include <QObject>
#include <QSharedPointer>

class QLockFile;
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

    /// A number of results data buffer.
    /// It's continuously incremented with every MeasureEvent.
    int num;
    
    /// A number of results in the buffer.
    /// It should be the same in each MeasureEvent (1000, as configured)
    /// but can be different when the measurement is stopped (less than 1000)
    int count;
    
    /// A pointer to the results data buffer.
    /// The buffer is located in the camera worker thread.
    /// The worker collects 1000 results to the buffer, then switches to another buffer,
    /// and sends the previous one in MeasureEvent to be saved into <measurement>.csv file.
    /// This allows for continuous capturing without buffer locking while saving.
    /// Even on 60FPS the capturing of 1000 results takes about 1/60*1000 ~ 16sec
    /// which is pretty enough for saving data to the results file before buffer swaps.
    /// It's supposed that the file is in a local folder and opened exclusively for writing
    /// so no one should interfere and slow down the writing.
    Measurement *results;

    /// Arbitrary info about the measurement.
    /// It's saved into <measurement>.ini file 
    QMap<QString, QVariant> stats;
    
    bool last = false;
    bool finished = false;
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

signals:
    void finished();
    void failed(const QString &error);

protected:
    bool event(QEvent *event) override;

private:
    QString _id;
    /// Measurement session start time
    QDateTime _measureStart;
    QSharedPointer<QThread> _thread;
    MeasureConfig _config;
    QString _cfgFile, _imgDir;
    QMap<qint64, QString> _errors;
    int _width, _height, _bpp;
    double _scale = 1;
    qint64 _intervalBeg;
    qint64 _intervalLen;
    int _intervalIdx;
    double _avg_xc, _avg_yc, _avg_dx, _avg_dy, _avg_phi;
    double _avg_cnt;
    QMap<int, double> _multires_avg;
    QMap<int, int> _multires_avg_cnt;
    int _multires_cnt = 0;
    int _savedImgCount = 0;
    qint64 _elapsedSecs = 0;
    QList<int> _auxCols;
    QMap<int, double> _auxAvgVals;
    double _auxAvgCnt;
    qint64 _prevFrameTime;
    std::unique_ptr<CsvFile> _csvFile;
    std::unique_ptr<QLockFile> _lockFile;
    QString _failure;
    bool _isFinished = false;

    QString checkConfig();
    QString acquireLock();
    QString saveIniFile(Camera *cam);
    QString prepareCsvFile(Camera *cam);
    QString prepareImagesDir();
    void processMeasure(MeasureEvent *e);
    void saveImage(ImageEvent *e);
    void saveStats(MeasureEvent *e);
    void stopFail(const QString &error);
    
    void calcIntervalAverage(QTextStream &out, const Measurement &r);

    template <typename T>
    QString formatTime(qint64 time, T fmt) {
        return QDateTime::fromMSecsSinceEpoch(time).toString(fmt);
    }
};

#endif // MEASURE_SAVER_H
