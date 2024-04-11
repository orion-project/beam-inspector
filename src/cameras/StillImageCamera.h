#ifndef STILL_IMAGE_CAMERA_H
#define STILL_IMAGE_CAMERA_H

#include "cameras/CameraBase.h"

#include <QSharedPointer>
#include <QString>

class PlotIntf;
class TableIntf;

class StillImageCamera
{
public:
    static bool editSettings();
    static std::optional<CameraInfo> start(PlotIntf *plot, TableIntf *table);
    static std::optional<CameraInfo> start(const QString& fileName, PlotIntf *plot, TableIntf *table);
};

#endif // STILL_IMAGE_CAMERA_H
