#ifndef DLL_CAMERA_H
#define DLL_CAMERA_H

#include "cameras/Camera.h"

#include "core/OriResult.h"

#include <QThread>

class DllIntf;

class DllCamera final : public QThread, public Camera
{
    Q_OBJECT

public:
    DllCamera(const QString& customId, PlotIntf *plot, TableIntf *table, StabilityIntf *stabil, QObject *parent);
    ~DllCamera();
    
    QString name() const override { return _name; }
    int width() const override { return _width; }
    int height() const override { return _height; }
    int bpp() const override { return _bpp; }
    QString customId() const override { return _customId; }
    PixelScale sensorScale() const override { return _pixelScale; }
    TableRowsSpec tableRows() const override;
    
    bool isCapturing() const override { return (bool)_dll; }
    void startCapture() override;
    void stopCapture() override;
    
    bool canSaveRawImg() const override { return true; }
    void requestRawImg(QObject *sender) override;
    void setRawView(bool on, bool reconfig) override;
    
    static void loadAdapters();
    static void unloadAdapters();

    struct AdapterItem
    {
        QString id;
        QString name;
        QString error;
    };
    static QVector<AdapterItem> getAdapters();

    using CamerasResult = Ori::Result<QVector<CameraItem>>;
    static CamerasResult getCameras(const QString& adapterId);

signals:
    void ready();
    void stats(const CameraStats& stats);
    void error(const QString& err);
    
protected:
    void run() override;
    
private slots:
    void camConfigChanged();
    
private:
    QString _name, _customId;
    int _width = 0, _height = 0, _bpp = 0;
    PixelScale _pixelScale;
    QSharedPointer<DllIntf> _dll;
    friend class DllIntf;
};

# endif // DLL_CAMERA_H
