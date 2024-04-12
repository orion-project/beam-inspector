#ifndef VIRTUAL_DEMO_CAMERA_H
#define VIRTUAL_DEMO_CAMERA_H

#include "cameras/Camera.h"

#include <QThread>
#include <QSharedPointer>

class BeamRenderer;
class PlotIntf;
class TableIntf;

class VirtualDemoCamera : public QThread
{
    Q_OBJECT

public:
    explicit VirtualDemoCamera(PlotIntf *plot, TableIntf *table, QObject *parent = nullptr);

    static CameraInfo info();

signals:
    void ready();
    void stats(int fps);

protected:
    void run() override;

private:
    QSharedPointer<BeamRenderer> _render;
};

#endif // VIRTUAL_DEMO_CAMERA_H
