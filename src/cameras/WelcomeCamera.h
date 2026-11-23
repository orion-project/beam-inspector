#ifndef WELCOME_CAMERA_H
#define WELCOME_CAMERA_H

#include "cameras/Camera.h"

class WelcomeCamera : public Camera
{
public:
    WelcomeCamera(PlotIntf *plot, TableIntf *table, StabilityIntf *stabil);

    QString name() const override { return "Welcome"; }
    int width() const override { return 80; }
    int height() const override { return 80; }
    int bpp() const override { return 8; }

    void startCapture() override;
    
    bool hasStability() const override { return false; }

    ControlPanel* controlPanel(QWidget *parent) override;

private:
    ControlPanel* _controlPanel = nullptr;
};

#endif // WELCOME_CAMERA_H
