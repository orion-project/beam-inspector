#ifndef STILL_IMAGE_CAMERA_H
#define STILL_IMAGE_CAMERA_H

#include "cameras/Camera.h"

#include <QImage>

class StillImageCamera : public Camera
{
public:
    StillImageCamera(PlotIntf *plot, TableIntf *table);
    StillImageCamera(PlotIntf *plot, TableIntf *table, const QString& fileName);

    QString name() const override;
    QString descr() const override;
    int width() const override;
    int height() const override;
    int bpp() const override;
    QList<QPair<int, QString>> dataRows() const override;

    QString fileName() const { return _fileName; }

    void startCapture() override;

    void setRawView(bool on, bool reconfig) override;

private:
    QString _fileName;
    QImage _image;
    bool _rawView = false;
};

#endif // STILL_IMAGE_CAMERA_H
