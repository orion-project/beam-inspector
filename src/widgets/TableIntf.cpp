#include "TableIntf.h"

#include <QTableWidgetItem>

void TableIntf::setResult(const CgnBeamResult& r, double renderTime, double calcTime)
{
    _res = r;
    _renderTime = renderTime;
    _calcTime = calcTime;
}

void TableIntf::showData()
{
    double eps = qMin(_res.dx, _res.dy) / qMax(_res.dx, _res.dy);

    itXc->setText(QStringLiteral(" %1 ").arg((int)_res.xc));
    itYc->setText(QStringLiteral(" %1 ").arg((int)_res.yc));
    itDx->setText(QStringLiteral(" %1 ").arg((int)_res.dx));
    itDy->setText(QStringLiteral(" %1 ").arg((int)_res.dy));
    itPhi->setText(QStringLiteral(" %1° ").arg(_res.phi, 0, 'f', 1));
    itEps->setText(QStringLiteral(" %1 ").arg(eps, 0, 'f', 3));

    itRenderTime->setText(QStringLiteral(" %1 ms ").arg(qRound(_renderTime)));
    itCalcTime->setText(QStringLiteral(" %1 ms ").arg(qRound(_calcTime)));
}
