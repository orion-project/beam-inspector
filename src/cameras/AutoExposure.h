#ifndef AUTO_EXPOSURE_H
#define AUTO_EXPOSURE_H

#include <QString>

class AutoExposureImpl;

class AutoExposure
{
public:
    const char* logId = nullptr;
    double targetLevel = 0;
    int avgFrames = 0;
    std::function<QString(double*, double*, double*)> getExpoRange;
    std::function<QString(double*, double*, double*)> getFpsRange;
    std::function<QString(double)> setExpo;
    std::function<QString(double*)> getExpo;
    std::function<QString(double)> setFps;
    std::function<QString(double*)> getFps;
    std::function<void()> stepRequested;
    std::function<void(double, double)> expoChanged;
    std::function<void()> finished;

    bool start();
    void doStep(double level);
    void copyLog();
    
private:
    std::shared_ptr<AutoExposureImpl> _impl;
};

#endif // AUTO_EXPOSURE_H
