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

protected:
    bool event(QEvent *event) override;

private:
    QSharedPointer<QThread> _thread;
    MeasureConfig _config;
    double _scale = 1;
    int _duration = 0;
    qint64 _startTime;
};

#endif // MEASURE_SAVER_H
