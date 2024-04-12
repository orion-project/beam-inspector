#ifndef WELCOME_CAMERA_H
#define WELCOME_CAMERA_H

#include "cameras/Camera.h"

class WelcomeCamera : public Camera
{
public:
    WelcomeCamera(PlotIntf *plot, TableIntf *table);

    QString name() const override { return "Welcome"; }
    int width() const override { return 80; }
    int height() const override { return 80; }
    int bits() const override { return 8; }

    void capture() override;
};

#endif // WELCOME_CAMERA_H
