#ifndef STABILITY_INTF_H
#define STABILITY_INTF_H

#include "beam_calc.h"

#include <QList>

class StabilityView;

class StabilityIntf
{
public:
    StabilityIntf(StabilityView *view);
    
    void setResult(qint64 time, const QList<CgnBeamResult>& val);
    
private:
    StabilityView *_view;
};

#endif // STABILITY_INTF_H
