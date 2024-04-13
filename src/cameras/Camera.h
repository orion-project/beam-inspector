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

    virtual void capture() = 0;

    CameraConfig config() const { return _config; };
    bool editConfig();

    void setAperture(const SoftAperture&);
    void toggleAperture(bool on);

    QString resolutionStr() const;

    enum ConfigPages { cfgPlot, cfgCalc, cfgAper };

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
