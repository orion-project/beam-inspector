#ifndef TABLE_INTF_H
#define TABLE_INTF_H

#include "beam_calc.h"

#include "cameras/CameraTypes.h"

class QTableWidgetItem;

class TableIntf
{
public:
    void cleanResult();
    void setResult(const CgnBeamResult& r, double renderTime, double calcTime);
    void showResult();
    void setScale(const PixelScale& scale) { _scale = scale; }

    QTableWidgetItem *itXc, *itYc, *itDx, *itDy, *itPhi, *itEps;
    QTableWidgetItem *itRenderTime, *itCalcTime;

private:
    PixelScale _scale;
    CgnBeamResult _res;
    double _renderTime, _calcTime;
};

#endif // TABLE_INTF_H
