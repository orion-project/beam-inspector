#ifndef STILL_IMAGE_CAMERA_H
#define STILL_IMAGE_CAMERA_H

#include <QSharedPointer>
#include <QString>

class BeamGraphIntf;
class TableIntf;

class StillImageCamera
{
public:
    struct ImageInfo
    {
        QString fileName;
        QString filePath;
        int width;
        int height;
        int bits;
    };

    static std::optional<ImageInfo> start(QSharedPointer<BeamGraphIntf> beam, QSharedPointer<TableIntf> table);
};

#endif // STILL_IMAGE_CAMERA_H
