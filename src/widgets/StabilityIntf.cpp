#include "StabilityIntf.h"

#include "widgets/StabilityView.h"

StabilityIntf::StabilityIntf(StabilityView *view): _view(view)
{

}

void StabilityIntf::setResult(qint64 time, const QList<CgnBeamResult>& val)
{
    _view->setResult(time, val);
}
