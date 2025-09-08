#ifndef IDS_CAMERA_H
#define IDS_CAMERA_H
#ifdef WITH_IDS

#include "cameras/Camera.h"

#include <QSharedPointer>
#include <QThread>

class HardConfigPanel;
class IdsCameraConfig;
class PeakIntf;

class IdsCamera final : public QThread, public Camera
{
    Q_OBJECT

public:
    IdsCamera(QVariant id, PlotIntf *plot, TableIntf *table, StabilityIntf *stabil, QObject *parent);
    ~IdsCamera();

    QString name() const override { return _name; }
    QString descr() const override { return _descr; }
    int width() const override { return _width; }
    int height() const override { return _height; }
    int bpp() const override;
    PixelScale sensorScale() const override;
    TableRowsSpec tableRows() const override;
    QList<QPair<int, QString>> measurCols() const override;

    bool isCapturing() const override { return (bool)_peak; }
    void startCapture() override;
    void stopCapture() override;

    bool canMeasure() const override { return true; }
    void startMeasure(MeasureSaver *saver) override;
    void stopMeasure() override;

    void saveHardConfig(QSettings *s) override;
    HardConfigPanel* hardConfgPanel(QWidget *parent) override;

    bool canSaveRawImg() const override { return true; }
    void requestRawImg(QObject *sender) override;
    void setRawView(bool on, bool reconfig) override;

    bool isPowerMeter() const override { return true; }
    void togglePowerMeter() override;
    void raisePowerWarning() override;

    void requestExpWarning();

    bool canMavg() const override { return true; }

    QString customId() const override { return _customId; }

    static QVector<CameraItem> getCameras();
    static QString libError();
    static void unloadLib();

    IdsCameraConfig* idsConfig() { return _cfg.data(); }
    
    static QList<PixelFormat> pixelFormats();
    static QList<PixelFormat> supportedFormats();
    static PixelFormat supportedPixelFormat_Mono8();
    static PixelFormat supportedPixelFormat_Mono10();
    static PixelFormat supportedPixelFormat_Mono12();
    static PixelFormat supportedPixelFormat_Mono10G40();
    static PixelFormat supportedPixelFormat_Mono12G24();

signals:
    void ready();
    void stats(const CameraStats& stats);
    void error(const QString& err);

protected:
    void run() override;

    void initConfigMore(Ori::Dlg::ConfigDlgOpts &opts) override;
    void saveConfigMore(QSettings *s) override;
    void loadConfigMore(QSettings *s) override;

private slots:
    void camConfigChanged();

private:
    QString _name, _descr, _customId;
    int _width = 0;
    int _height = 0;
    PixelScale _pixelScale;
    QSharedPointer<PeakIntf> _peak;
    QSharedPointer<IdsCameraConfig> _cfg;
    HardConfigPanel *_configPanel = nullptr;
    friend class PeakIntf;
};

#endif // WITH_IDS
#endif // IDS_CAMERA_H
