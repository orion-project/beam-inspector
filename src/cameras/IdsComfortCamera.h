#ifndef IDS_COMFORT_CAMERA_H
#define IDS_COMFORT_CAMERA_H

#include "cameras/Camera.h"

#include <QSharedPointer>
#include <QThread>

class PeakIntf;

class IdsComfortCamera : public QThread, public Camera
{
    Q_OBJECT

public:
    IdsComfortCamera(PlotIntf *plot, TableIntf *table, QObject *parent);
    ~IdsComfortCamera();

    QString name() const override;
    int width() const override;
    int height() const override;
    int bits() const override;
    PixelScale sensorScale() const override;

    void startCapture() override;
    void startMeasure(QObject *saver);
    void stopMeasure();

signals:
    void ready();
    void stats(int fps, qint64 measureTime);
    void error(const QString& err);

protected:
    void run() override;

private slots:
    void camConfigChanged();

private:
    QSharedPointer<PeakIntf> _peak;
};

#endif // IDS_COMFORT_CAMERA_H
