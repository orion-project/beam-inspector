#ifndef TABLE_INTF_H
#define TABLE_INTF_H

#include "beam_calc.h"

class QTableWidgetItem;

class TableIntf
{
public:
    void cleanResult();
    void setResult(const CgnBeamResult& r, double renderTime, double calcTime);
    void showResult();

    QTableWidgetItem *itXc, *itYc, *itDx, *itDy, *itPhi, *itEps;
    QTableWidgetItem *itRenderTime, *itCalcTime;

private:
    CgnBeamResult _res;
    double _renderTime, _calcTime;
};

#endif // TABLE_INTF_H
