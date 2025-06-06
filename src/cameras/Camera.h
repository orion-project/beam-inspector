#ifndef CAMERA_BASE_H
#define CAMERA_BASE_H

#include <QString>

#include "cameras/CameraTypes.h"

class HardConfigPanel;
class MeasureSaver;
class PlotIntf;
class StabilityIntf;
class TableIntf;

class QWidget;

namespace Ori::Dlg {
struct ConfigDlgOpts;
}

class Camera
{
public:
    virtual ~Camera() {}

    virtual QString name() const = 0;
    virtual QString descr() const { return {}; }
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual int bpp() const = 0;
    virtual PixelScale sensorScale() const { return {}; }

    virtual bool isCapturing() const { return true; }
    virtual void startCapture() = 0;
    virtual void stopCapture() {}

    virtual bool canMeasure() const { return false; }
    virtual void startMeasure(MeasureSaver*) {}
    virtual void stopMeasure() {}

    virtual void saveHardConfig(QSettings*) {}
    virtual HardConfigPanel* hardConfgPanel(QWidget *parent) { return nullptr; }

    virtual bool canSaveRawImg() const { return false; }
    virtual void requestRawImg(QObject *sender) {}
    virtual void setRawView(bool on, bool reconfig) {}

    virtual bool isPowerMeter() const { return false; }
    virtual void togglePowerMeter() {}
    virtual void raisePowerWarning() {}
    bool setupPowerMeter();

    virtual QString customId() const { return {}; }

    /// Data rows to be show in the Result table
    virtual TableRowsSpec tableRows() const;

    /// Additional data cols to be saved in measurement file
    virtual QList<QPair<int, QString>> measurCols() const { return {}; }

    const CameraConfig& config() const { return _config; }
    enum ConfigPages {
        cfgPlot, cfgTable, cfgBgnd, cfgCentr, cfgRoi, cfgStabil,
        cfgPageCount
    };
    bool editConfig(int page = -1);

    virtual bool canMavg() const { return false; }
    virtual bool hasStability() const { return true; }

    void setRoi(const RoiRect&);
    void setRois(const QList<RoiRect>&);
    void setRois(const QList<QPointF>&);
    void setRoisSize(const FrameSize&);
    void setRoiMode(RoiMode mode);
    void setColorMap(const QString& colorMap);
    bool isRoiValid() const;
    bool editRoisSize();

    PixelScale pixelScale() const;
    QString resolutionStr() const;
    QString formatBrightness(double v) const;

    void loadConfig();
    void saveConfig(bool saveMore = false);

protected:
    PlotIntf *_plot;
    TableIntf *_table;
    StabilityIntf *_stabil;
    QString _configGroup;
    CameraConfig _config;

    Camera(PlotIntf *plot, TableIntf *table, StabilityIntf *stabil, const char* configGroup);

    virtual void initConfigMore(Ori::Dlg::ConfigDlgOpts &opts) {}
    virtual void loadConfigMore(QSettings*) {}
    virtual void saveConfigMore(QSettings*) {}
};

#endif // CAMERA_BASE_H
