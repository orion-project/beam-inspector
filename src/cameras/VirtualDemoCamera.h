#ifndef VIRTUAL_DEMO_CAMERA_H
#define VIRTUAL_DEMO_CAMERA_H

#include "cameras/Camera.h"

#include <QSharedPointer>
#include <QThread>

class BeamRenderer;

class VirtualDemoCamera : public QThread, public Camera
{
    Q_OBJECT

public:
    VirtualDemoCamera(PlotIntf *plot, TableIntf *table, StabilityIntf *stabil, QObject *parent);

    QString name() const override { return "Demo (render)"; }
    int width() const override;
    int height() const override;
    int bpp() const override { return 8; }
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

private slots:
    void camConfigChanged();

private:
    QSharedPointer<BeamRenderer> _render;
};

#endif // VIRTUAL_DEMO_CAMERA_H
