#include "BeamGraph.h"

BeamColorMapData::BeamColorMapData(int w, int h)
    : QCPColorMapData(w, h, QCPRange(0, w), QCPRange(0, h))
{
}


BeamEllipse::BeamEllipse(QCustomPlot *parentPlot) : QCPAbstractItem(parentPlot)
{
}

void BeamEllipse::draw(QCPPainter *painter)
{
    double x = parentPlot()->xAxis->coordToPixel(xc);
    double y = parentPlot()->yAxis->coordToPixel(yc);
    double rx = parentPlot()->xAxis->coordToPixel(dx/2.0);
    double ry = parentPlot()->yAxis->coordToPixel(dy/2.0);
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
