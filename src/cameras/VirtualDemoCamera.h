#ifndef VIRTUAL_DEMO_CAMERA_H
#define VIRTUAL_DEMO_CAMERA_H

#include "cameras/Camera.h"

#include <QThread>
#include <QSharedPointer>

class BeamRenderer;

class VirtualDemoCamera : public QThread, public Camera
{
    Q_OBJECT

public:
    VirtualDemoCamera(PlotIntf *plot, TableIntf *table, QObject *parent);

    QString name() const override { return "Demo"; }
    int width() const override;
    int height() const override;
    int bits() const override { return 8; }
    PixelScale sensorScale() const override { return { .on=true, .factor=2.5, .unit="um" }; }

    void capture() override;

signals:
    void ready();
    void stats(int fps);

protected:
    void run() override;

private:
    QSharedPointer<BeamRenderer> _render;
};

#endif // VIRTUAL_DEMO_CAMERA_H
