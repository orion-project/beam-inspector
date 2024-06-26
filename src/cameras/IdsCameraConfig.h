#ifndef IDS_CAMERA_CONFIG_H
#define IDS_CAMERA_CONFIG_H
#ifdef WITH_IDS

#include <ids_peak_comfort_c/ids_peak_comfort_c.h>

#include <QApplication>

class QSettings;

namespace Ori::Dlg {
struct ConfigDlgOpts;
}

class IdsCameraConfig
{
     Q_DECLARE_TR_FUNCTIONS(IdsCameraConfig)

public:
    struct FactorXY
    {
        uint32_t x = 1;
        uint32_t y = 1;
        QList<uint32_t> xs;
        QList<uint32_t> ys;
        bool configurable = false;
        QString str() const { return QStringLiteral("x=%1, y=%2").arg(x).arg(y); }
        bool on() const { return x > 1 or y > 1; }
        void reset() { x = 1, y = 1; }
    };

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
    FactorXY binning, decimation;
    QSet<int> supportedBpp;

    void initDlg(peak_camera_handle hCam, Ori::Dlg::ConfigDlgOpts &opts, int maxPageId);
    void save(QSettings *s);
    void load(QSettings *s);
};

#endif // WITH_IDS
#endif // IDS_CAMERA_CONFIG_H
