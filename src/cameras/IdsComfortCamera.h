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
    int width() const override { return _width; }
    int height() const override { return _height; }
    int bits() const override { return _bits; }
    PixelScale sensorScale() const override;

    void startCapture() override;
    void stopCapture() override;
    void startMeasure(QObject *saver) override;
    void stopMeasure() override;

signals:
    void ready();
    void stats(const CameraStats& stats);
    void error(const QString& err);

protected:
    void run() override;

private slots:
    void camConfigChanged();

private:
    int _width, _height, _bits;
    QSharedPointer<PeakIntf> _peak;
    friend class PeakIntf;
};

#endif // IDS_COMFORT_CAMERA_H
