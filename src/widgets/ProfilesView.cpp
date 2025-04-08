#include "ProfilesView.h"

#include "helpers/OriLayouts.h"

#include "qcustomplot/src/core.h"
#include "qcustomplot/src/layoutelements/layoutelement-axisrect.h"
#include "qcustomplot/src/plottables/plottable-graph.h"

ProfilesView::ProfilesView(QWidget *parent) : QWidget{parent}
{
    _plot = new QCustomPlot;

    _axisRectY = new QCPAxisRect(_plot);
    _plot->plotLayout()->addElement(0, 1, _axisRectY);
    _plot->plotLayout()->setMargins(QMargins(12, 0, 12, 0));

    _graphX = _plot->addGraph();
    _graphY = _plot->addGraph(_axisRectY->axis(QCPAxis::atBottom), _axisRectY->axis(QCPAxis::atLeft));

    QVector<QCPGraphData> dataGauss(50);
    for (int i = 0; i < dataGauss.size(); ++i)
    {
        dataGauss[i].key = i/(double)dataGauss.size()*10-5.0;
        dataGauss[i].value = qExp(-dataGauss[i].key*dataGauss[i].key*0.2)*1000;
    }
    _graphX->data()->set(dataGauss);
    _graphY->data()->set(dataGauss);
    _graphX->rescaleAxes();
    _graphY->rescaleAxes();

    setThemeColors(PlotHelpers::SYSTEM, false);

    Ori::Layouts::LayoutV({_plot}).setMargin(0).useFor(this);
}

void ProfilesView::setThemeColors(PlotHelpers::Theme theme, bool replot)
{
    PlotHelpers::setThemeColors(_plot, theme);
    PlotHelpers::setDefaultAxesFormat(_axisRectY, theme);
    if (replot) _plot->replot();
}
