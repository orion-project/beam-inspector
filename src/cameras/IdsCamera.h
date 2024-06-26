#ifndef IDS_CAMERA_H
#define IDS_CAMERA_H
#ifdef WITH_IDS

#include "cameras/Camera.h"

#include <QPointer>
#include <QSharedPointer>
#include <QThread>

class HardConfigPanel;
class IdsCameraConfig;
class PeakIntf;

class IdsCamera final : public QThread, public Camera
{
    Q_OBJECT

public:
    IdsCamera(QVariant id, PlotIntf *plot, TableIntf *table, QObject *parent);
    ~IdsCamera();

    QString name() const override { return _name; }
    QString descr() const override { return _descr; }
    int width() const override { return _width; }
    int height() const override { return _height; }
    int bpp() const override;
    PixelScale sensorScale() const override;
    QList<QPair<int, QString>> dataRows() const override;

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

    static QVector<CameraItem> getCameras();
    static void unloadLib();

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
    QVariant _id;
    QString _name, _descr;
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
