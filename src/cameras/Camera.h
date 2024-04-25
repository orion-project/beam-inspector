#ifndef CAMERA_BASE_H
#define CAMERA_BASE_H

#include <QString>

#include "cameras/CameraTypes.h"

class PlotIntf;
class TableIntf;

class Camera
{
public:
    virtual ~Camera() {}

    virtual QString name() const = 0;
    virtual QString descr() const { return {}; }
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual int bits() const = 0;
    virtual PixelScale sensorScale() const { return {}; }
    virtual bool isCapturing() const { return false; }
    virtual void startCapture() = 0;
    virtual void stopCapture() {}
    virtual void startMeasure(QObject *saver) {}
    virtual void stopMeasure() {}

    const CameraConfig& config() const { return _config; }
    enum ConfigPages { cfgPlot, cfgBgnd, cfgRoi };
    bool editConfig(int page = -1);
    
    void setAperture(const RoiRect&);
    void toggleAperture(bool on);
    bool isRoiValid() const;

    PixelScale pixelScale() const;
    QString resolutionStr() const;

protected:
    PlotIntf *_plot;
    TableIntf *_table;
    QString _configGroup;
    CameraConfig _config;

    Camera(PlotIntf *plot, TableIntf *table, const char* configGroup);

private:
    void loadConfig();
    void saveConfig();
};

#endif // CAMERA_BASE_H
