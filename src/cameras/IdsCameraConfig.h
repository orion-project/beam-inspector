#ifndef IDS_CAMERA_CONFIG_H
#define IDS_CAMERA_CONFIG_H
#ifdef WITH_IDS

#include <ids_peak_comfort_c/ids_peak_comfort_c.h>

#include <QApplication>

namespace Ori::Dlg {
struct ConfigDlgOpts;
}

class IdsCameraConfig
{
     Q_DECLARE_TR_FUNCTIONS(IdsCameraConfig)

public:
    int bpp = 0;
    bool bpp8, bpp10, bpp12;
    bool intoRequested = false;
    QString infoModelName;
    QString infoFamilyName;
    QString infoSerialNum;
    QString infoVendorName;
    QString infoManufacturer;
    QString infoDeviceVer;
    QString infoFirmwareVer;
    QList<quint32> binningsX, binningsY;
    quint32 binningX = 0, binningY = 0;
    QList<quint32> decimsX, decimsY;
    quint32 decimX = 0, decimY = 0;
    QSet<int> supportedBpp;

    void initDlg(peak_camera_handle hCam, Ori::Dlg::ConfigDlgOpts &opts, int maxPageId);
    void save(const QString& group);
    void load(const QString& group);
};

#endif // WITH_IDS
#endif // IDS_CAMERA_CONFIG_H
