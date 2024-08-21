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
    VirtualDemoCamera(PlotIntf *plot, TableIntf *table, QObject *parent);

    QString name() const override { return "Demo"; }
    int width() const override;
    int height() const override;
    int bpp() const override { return 8; }
    PixelScale sensorScale() const override { return { .on=true, .factor=2.5, .unit="um" }; }
    QList<QPair<int, QString>> dataRows() const override;

    void startCapture() override;

    bool canMeasure() const override { return true; }
    void startMeasure(MeasureSaver *saver) override;
    void stopMeasure() override;

    bool canSaveRawImg() const override { return true; }
    void requestRawImg(QObject *sender) override;
    void setRawView(bool on, bool reconfig) override;

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
