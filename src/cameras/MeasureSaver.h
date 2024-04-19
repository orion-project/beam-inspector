#ifndef MEASURE_SAVER_H
#define MEASURE_SAVER_H

#include <QObject>
#include <QSharedPointer>

class MeasureSaver : public QObject
{
    Q_OBJECT

public:
    MeasureSaver();
    ~MeasureSaver();

    QString start();

signals:
    void finished();

protected:
    bool event(QEvent *event) override;

private:
    QSharedPointer<QThread> _thread;
    QString _fileName;
    double _scale = 1;
};

#endif // MEASURE_SAVER_H
