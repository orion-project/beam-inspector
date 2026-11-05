#ifndef DLL_CAMERA_H
#define DLL_CAMERA_H

#include "cameras/Camera.h"

#include <QThread>

class DllIntf;

class DllCamera final : public QThread, public Camera
{
public:
    DllCamera(QVariant id, PlotIntf *plot, TableIntf *table, StabilityIntf *stabil, QObject *parent);
    ~DllCamera();
    
    QString name() const override { return _name; }
    int width() const override { return _width; }
    int height() const override { return _height; }
    int bpp() const override { return _bpp; }
    QString customId() const override { return _customId; }
    
    bool isCapturing() const override { return (bool)_dll; }
    void startCapture() override;
    void stopCapture() override;
    
    struct AdapterItem
    {
        QString id;
        QString name;
        QString error;
    };
    
    static void loadAdapters(QSettings*);
    static void unloadAdapters();
    static QVector<AdapterItem> getAdapters();
    static QVector<CameraItem> getCameras(const QString& adapterId);
    
private:
    QString _name, _descr, _customId;
    int _width = 0, _height = 0, _bpp = 0;
    QSharedPointer<DllIntf> _dll;
};

# endif // DLL_CAMERA_H