#include "TableIntf.h"

#include <QTableWidgetItem>

void TableIntf::cleanResult()
{
    memset(&_res, 0, sizeof(CgnBeamResult));
    _acqTime = 0;
    _calcTime = 0;
}

void TableIntf::setResult(const CgnBeamResult& r, double acqTime, double calcTime)
{
    _res = r;
    _acqTime = acqTime;
    _calcTime = calcTime;
}

inline void setTextInvald(QTableWidgetItem *it)
{
    it->setText(QStringLiteral(" --- "));
}

void TableIntf::showResult()
{
    itAcqTime->setText(QStringLiteral(" %1 ms ").arg(qRound(_acqTime)));
    itCalcTime->setText(QStringLiteral(" %1 ms ").arg(qRound(_calcTime)));

    if (_res.nan)
    {
        setTextInvald(itXc);
        setTextInvald(itYc);
        setTextInvald(itDx);
        setTextInvald(itDy);
        setTextInvald(itPhi);
        setTextInvald(itEps);
        return;
    }

    double eps = qMin(_res.dx, _res.dy) / qMax(_res.dx, _res.dy);
    itXc->setText(_scale.formatWithMargins(_res.xc));
    itYc->setText(_scale.formatWithMargins(_res.yc));
    itDx->setText(_scale.formatWithMargins(_res.dx));
    itDy->setText(_scale.formatWithMargins(_res.dy));
    itPhi->setText(QStringLiteral(" %1° ").arg(_res.phi, 0, 'f', 1));
    itEps->setText(QStringLiteral(" %1 ").arg(eps, 0, 'f', 3));
}

bool TableIntf::resultInvalid() const
{
    return _res.nan;
}
