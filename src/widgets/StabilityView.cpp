#include "StabilityView.h"

#include "helpers/OriLayouts.h"
#include "helpers/OriWidgets.h"

#include "qcustomplot/src/core.h"
#include "qcustomplot/src/layoutelements/layoutelement-axisrect.h"
#include "qcustomplot/src/layoutelements/layoutelement-legend.h"
#include "qcustomplot/src/plottables/plottable-graph.h"

#define A_ Ori::Gui::action

StabilityView::StabilityView(QWidget *parent) : QWidget{parent}
{
    _plotTime = new QCustomPlot;
    _plotHeat = new QCustomPlot;

    _plotTime->setContextMenuPolicy(Qt::ActionsContextMenu);
    _plotHeat->setContextMenuPolicy(Qt::ActionsContextMenu);

    _plotTime->legend->setVisible(true);

    // By default, the legend is in the inset layout of the main axis rect.
    // So this is how we access it to change legend placement:
    _plotTime->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignLeft|Qt::AlignTop);
    
    _timelineX = _plotTime->addGraph();
    _timelineY = _plotTime->addGraph();
    _timelineX->setName("Center X");
    _timelineY->setName("Center Y");
    
    setThemeColors(PlotHelpers::SYSTEM, false);
    
    _plotTime->addAction(A_(tr("Copy X Points"), this, [this]{ PlotHelpers::copyGraph(_timelineX); }));
    _plotTime->addAction(A_(tr("Copy Y Points"), this, [this]{ PlotHelpers::copyGraph(_timelineY); }));
    
    _plotTime->addAction(A_(tr("Copy as Image"), this, [this]{
        PlotHelpers::copyImage(_plotTime, [this](PlotHelpers::Theme theme){ setThemeColors(theme, false); });
    }, ":/toolbar/copy_img"));
    
    _plotHeat->addAction(A_(tr("Copy as Image"), this, [this]{
        PlotHelpers::copyImage(_plotHeat, [this](PlotHelpers::Theme theme){ setThemeColors(theme, false); });
    }, ":/toolbar/copy_img"));
    
    Ori::Layouts::LayoutH({_plotTime, _plotHeat}).setMargins(12, 0, 12, 0).useFor(this);
}

void StabilityView::setThemeColors(PlotHelpers::Theme theme, bool replot)
{
    PlotHelpers::setThemeColors(_plotTime, theme);
    PlotHelpers::setThemeColors(_plotHeat, theme);
    _timelineX->setPen(QPen(QColor(0, 0, 255, 150), 2));
    _timelineY->setPen(QPen(QColor(255, 0, 0, 150), 2));
    if (replot) {
        _plotTime->replot();
        _plotHeat->replot();
    }
}

void StabilityView::restoreState(QSettings *s)
{
}

void StabilityView::setConfig(const PixelScale& scale)
{
    _scale = scale;
}

void StabilityView::showResult()
{

}

void StabilityView::cleanResult()
{

}
