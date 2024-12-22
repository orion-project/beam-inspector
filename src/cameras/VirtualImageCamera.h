#ifndef VIRTUAL_IMAGE_CAMERA_H
#define VIRTUAL_IMAGE_CAMERA_H

#include "cameras/Camera.h"

#include <QSharedPointer>
#include <QThread>

class ImageCameraWorker;

class VirtualImageCamera : public QThread, public Camera
{
    Q_OBJECT

public:
    VirtualImageCamera(PlotIntf *plot, TableIntf *table, QObject *parent);

    QString name() const override { return "Demo (image)"; }
    int width() const override;
    int height() const override;
    int bpp() const override;
    PixelScale sensorScale() const override { return { .on=true, .factor=2.5, .unit="um" }; }
    TableRowsSpec tableRows() const override;
    QList<QPair<int, QString>> measurCols() const override;

    void startCapture() override;

    bool canMeasure() const override { return true; }
    void startMeasure(MeasureSaver *saver) override;
    void stopMeasure() override;

    bool canSaveRawImg() const override { return true; }
    void requestRawImg(QObject *sender) override;
    void setRawView(bool on, bool reconfig) override;

    bool isPowerMeter() const override { return true; }
    void togglePowerMeter() override;
    void raisePowerWarning() override;

    bool canMavg() const override { return true; }

signals:
    void ready();
    void stats(const CameraStats &stats);

protected:
    void run() override;

    void initConfigMore(Ori::Dlg::ConfigDlgOpts &opts) override;
    void saveConfigMore(QSettings *s) override;
    void loadConfigMore(QSettings *s) override;

private slots:
    void camConfigChanged();

private:
    QSharedPointer<ImageCameraWorker> _render;
    QString _fileName;
    int _centerX = -1;
    int _centerY = -1;
    int _jitterAngle = 0;
    int _jitterShift = 0;
    friend class ImageCameraWorker;
};

#endif // VIRTUAL_IMAGE_CAMERA_H
