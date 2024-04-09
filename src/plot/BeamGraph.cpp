#include "BeamGraph.h"

#include "qcp/src/core.h"
#include "qcp/src/painter.h"

//------------------------------------------------------------------------------
//                               BeamColorMapData
//------------------------------------------------------------------------------

BeamColorMapData::BeamColorMapData(int w, int h)
    : QCPColorMapData(w, h, QCPRange(0, w), QCPRange(0, h))
{
}

//------------------------------------------------------------------------------
//                                BeamEllipse
//------------------------------------------------------------------------------

BeamEllipse::BeamEllipse(QCustomPlot *parentPlot) : QCPAbstractItem(parentPlot)
{
}

void BeamEllipse::draw(QCPPainter *painter)
{
    double x = parentPlot()->xAxis->coordToPixel(xc);
    double y = parentPlot()->yAxis->coordToPixel(yc);
    double rx = parentPlot()->xAxis->coordToPixel(xc + dx/2.0) - x;
    double ry = parentPlot()->yAxis->coordToPixel(yc + dy/2.0) - y;
    auto t = painter->transform();
    painter->translate(x, y);
    painter->rotate(phi);
    painter->setPen(pen);
    painter->drawEllipse(QPointF(), rx, ry);
    painter->setTransform(t);
}

double BeamEllipse::selectTest(const QPointF &pos, bool onlySelectable, QVariant *details) const
{
    return 0;
}

//------------------------------------------------------------------------------
//                               BeamColorScale
//------------------------------------------------------------------------------

BeamColorScale::BeamColorScale(QCustomPlot *parentPlot) : QCPColorScale(parentPlot)
{
}

void BeamColorScale::setFrameColor(const QColor& c)
{
    for (auto a : mAxisRect->axes())
        a->setBasePen(QPen(c, 0, Qt::SolidLine));
}
