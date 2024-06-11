#ifndef IDS_COMFORT_CAMERA_H
#define IDS_COMFORT_CAMERA_H

#ifdef WITH_IDS

#include "cameras/Camera.h"

#include <QPointer>
#include <QSharedPointer>
#include <QThread>

class HardConfigPanel;
class PeakIntf;

class IdsComfort
{
public:
    static IdsComfort* init();
    ~IdsComfort();

    QVector<CameraItem> getCameras();

private:
    IdsComfort();
};

class IdsComfortCamera : public QThread, public Camera
{
    Q_OBJECT

public:
    IdsComfortCamera(QVariant id, PlotIntf *plot, TableIntf *table, QObject *parent);
    ~IdsComfortCamera();

    QString name() const override { return _name; }
    QString descr() const override { return _descr; }
    int width() const override { return _width; }
    int height() const override { return _height; }
    int bpp() const override { return _bpp; }
    PixelScale sensorScale() const override;

    bool isCapturing() const override { return (bool)_peak; }
    void startCapture() override;
    void stopCapture() override;

    bool canMeasure() const override { return true; }
    void startMeasure(MeasureSaver *saver) override;
    void stopMeasure() override;

    void saveHardConfig(QSettings *s) override;
    HardConfigPanel* hardConfgPanel(QWidget *parent) override;

    void requestRawImg(QObject *sender) override;
    void setRawView(bool on, bool reconfig) override;

    QVariant id() const { return _id; }

signals:
    void ready();
    void stats(const CameraStats& stats);
    void error(const QString& err);

protected:
    void run() override;

    void initConfigMore(Ori::Dlg::ConfigDlgOpts &opts) override;
    void saveConfigMore() override;
    void loadConfigMore();

private slots:
    void camConfigChanged();

private:
    QVariant _id;
    QString _name, _descr;
    int _width = 0;
    int _height = 0;
    int _bpp = 0;
    PixelScale _pixelScale;
    QSharedPointer<PeakIntf> _peak;
    HardConfigPanel *_configPanel = nullptr;
    friend class PeakIntf;

    struct ConfigEditorData
    {
        bool bpp8, bpp10, bpp12;
        bool intoRequested = false;
        QString infoModelName;
        QString infoFamilyName;
        QString infoSerialNum;
        QString infoVendorName;
        QString infoManufacturer;
        QString infoDeviceVer;
        QString infoFirmwareVer;
    };
    ConfigEditorData _cfg;
};

#endif // WITH_IDS
#endif // IDS_COMFORT_CAMERA_H
