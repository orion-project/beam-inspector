#ifndef MEASURE_SAVER_H
#define MEASURE_SAVER_H

#include <QObject>
#include <QSharedPointer>

class QSettings;

class Camera;

struct MeasureConfig
{
    QString fileName;
    bool allFrames;
    int intervalSecs;
    bool average;
    bool durationInf;
    QString duration;

    void load(QSettings *s);
    void save(QSettings *s, bool min=false) const;
    void loadRecent();
    void saveRecent() const;
    int durationSecs() const;
};

class MeasureSaver : public QObject
{
    Q_OBJECT

public:
    static std::optional<MeasureConfig> configure();

    MeasureSaver();
    ~MeasureSaver();

    QString start(const MeasureConfig &cfg, Camera* cam);

signals:
    void finished();
    void interrupted(const QString &error);

protected:
    bool event(QEvent *event) override;

private:
    QSharedPointer<QThread> _thread;
    MeasureConfig _config;
    double _scale = 1;
    int _duration = 0;
    qint64 _startTime;
    qint64 _interval_beg;
    qint64 _interval_len;
    int _interval_idx;
    double _avg_xc, _avg_yc, _avg_dx, _avg_dy, _avg_phi, _avg_eps;
    double _avg_cnt;
};

#endif // MEASURE_SAVER_H
