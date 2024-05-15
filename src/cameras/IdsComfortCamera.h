#ifndef IDS_COMFORT_CAMERA_H
#define IDS_COMFORT_CAMERA_H

#include "cameras/Camera.h"

#include <QPointer>
#include <QSharedPointer>
#include <QThread>

class PeakIntf;

class IdsComfort
{
public:
    static IdsComfort* init();
    ~IdsComfort();

    QVector<CameraItem> getCameras();

private:
    IdsComfort();
};

class IdsComfortCamera : public QThread, public Camera
{
    Q_OBJECT

public:
    IdsComfortCamera(QVariant id, PlotIntf *plot, TableIntf *table, QObject *parent);
    ~IdsComfortCamera();

    QString name() const override { return _name; }
    QString descr() const override { return _descr; }
    int width() const override { return _width; }
    int height() const override { return _height; }
    int bits() const override { return _bits; }
    PixelScale sensorScale() const override;
    bool isCapturing() const override { return (bool)_peak; }
    void startCapture() override;
    void stopCapture() override;
    void startMeasure(MeasureSaver *saver) override;
    void stopMeasure() override;
    void saveHardConfig(QSettings *s) override;
    void requestRawImg(QObject *sender) override;

    QVariant id() const { return _id; }

    QPointer<QWidget> showHardConfgWindow();

signals:
    void ready();
    void stats(const CameraStats& stats);
    void error(const QString& err);

protected:
    void run() override;

private slots:
    void camConfigChanged();

private:
    QVariant _id;
    QString _name, _descr;
    int _width = 0;
    int _height = 0;
    int _bits = 0;
    QSharedPointer<PeakIntf> _peak;
    QPointer<QWidget> _cfgWnd;
    friend class PeakIntf;
};

#endif // IDS_COMFORT_CAMERA_H
