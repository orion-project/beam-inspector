#ifndef STILL_IMAGE_CAMERA_H
#define STILL_IMAGE_CAMERA_H

#include "cameras/CameraBase.h"

#include <QSharedPointer>
#include <QString>

class BeamGraphIntf;
class TableIntf;

class StillImageCamera
{
public:
    static std::optional<CameraInfo> start(QSharedPointer<BeamGraphIntf> beam, QSharedPointer<TableIntf> table);
    static std::optional<CameraInfo> start(const QString& fileName, QSharedPointer<BeamGraphIntf> beam, QSharedPointer<TableIntf> table);
};

#endif // STILL_IMAGE_CAMERA_H
