#ifndef STILL_IMAGE_CAMERA_H
#define STILL_IMAGE_CAMERA_H

#include "cameras/Camera.h"

#include <QImage>

class StillImageCamera : public Camera
{
public:
    StillImageCamera(PlotIntf *plot, TableIntf *table, StabilityIntf *stabil);
    StillImageCamera(PlotIntf *plot, TableIntf *table, StabilityIntf *stabil, const QString& fileName);

    QString name() const override;
    QString descr() const override;
    int width() const override;
    int height() const override;
    int bpp() const override;
    TableRowsSpec tableRows() const override;

    QString fileName() const { return _fileName; }
    bool isDemoMode() const { return _demoMode; }
    
    bool hasStability() const override { return false; }

    void startCapture() override;

    void setRawView(bool on, bool reconfig) override;

private:
    QString _fileName;
    QImage _image;
    bool _rawView = false;
    bool _demoMode = false;
};

#endif // STILL_IMAGE_CAMERA_H
