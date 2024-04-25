#ifndef IDS_COMFORT_H
#define IDS_COMFORT_H

#include "cameras/CameraTypes.h"

class IdsComfort
{
public:
    static IdsComfort* init();
    ~IdsComfort();

    QVector<CameraItem> getCameras();

private:
    IdsComfort();
};

#endif // IDS_COMFORT_H
