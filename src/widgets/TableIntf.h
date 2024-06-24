#ifndef TABLE_INTF_H
#define TABLE_INTF_H

#include "beam_calc.h"

#include "cameras/CameraTypes.h"

class QTableWidgetItem;

class TableIntf
{
public:
    void setScale(const PixelScale& scale) { _scale = scale; }
    void setResult(const CgnBeamResult& r, double acqTime, double calcTime);
    void cleanResult();
    void showResult();
    bool resultInvalid() const;

    QTableWidgetItem *itXc, *itYc, *itDx, *itDy, *itPhi, *itEps;
    QTableWidgetItem *itAcqTime, *itCalcTime;

private:
    PixelScale _scale;
    CgnBeamResult _res;
    double _acqTime, _calcTime;
};

#endif // TABLE_INTF_H
