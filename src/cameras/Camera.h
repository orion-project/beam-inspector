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

    const CameraConfig& config() const { return _config; };
    enum ConfigPages { cfgPlot, cfgBgnd, cfgAper };
    bool editConfig(ConfigPages page = cfgPlot);

    void setAperture(const SoftAperture&);
    void toggleAperture(bool on);
    bool isApertureValid() const;

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
