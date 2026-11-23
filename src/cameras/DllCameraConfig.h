#ifndef DLL_CAMERA_CONFIG_H
#define DLL_CAMERA_CONFIG_H

#include "CameraTypes.h"

#include <QApplication>

class QSettings;

namespace Ori::Dlg {
struct ConfigDlgOpts;
}

class DllCameraConfig
{
    Q_DECLARE_TR_FUNCTIONS(DllCameraConfig)

public:
    int pixelFormat = 0;
    QList<PixelFormat> availablePixelFormats;

    void initDlg(Ori::Dlg::ConfigDlgOpts &opts, int maxPageId);
    void save(QSettings *s);
    void load(QSettings *s);
};

#endif // DLL_CAMERA_CONFIG_H