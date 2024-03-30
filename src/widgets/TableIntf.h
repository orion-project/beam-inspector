#ifndef TABLE_INTF_H
#define TABLE_INTF_H

#include "beam_calc.h"

class QTableWidgetItem;

class TableIntf
{
public:
    void setResult(const CgnBeamResult& r, double renderTime, double calcTime);
    void showData();

    QTableWidgetItem *itXc, *itYc, *itDx, *itDy, *itPhi;
    QTableWidgetItem *itRenderTime, *itCalcTime;

private:
    CgnBeamResult _res;
    double _renderTime, _calcTime;
};

#endif // TABLE_INTF_H
