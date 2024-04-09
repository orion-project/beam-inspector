#ifndef BEAM_COLOR_MAP_H
#define BEAM_COLOR_MAP_H

#include "qcustomplot/item.h"
#include "qcustomplot/plottables/plottable-colormap.h"

/**
 * A thin wrapper around QCPColorMapData providing access to protected fields
 */
class BeamColorMapData : public QCPColorMapData
{
public:
    BeamColorMapData(int w, int h);

    inline double* rawData() { return mData; }
    inline void invalidate() { mDataModified = true; }
};

class BeamEllipse : public QCPAbstractItem
{
public:
    explicit BeamEllipse(QCustomPlot *parentPlot);

    QPen pen;
    double xc, yc, dx, dy, phi;

    double selectTest(const QPointF &pos, bool onlySelectable, QVariant *details=nullptr) const override;

protected:
    void draw(QCPPainter *painter) override;
};

class BeamColorScale : public QCPColorScale
{
public:
    explicit BeamColorScale(QCustomPlot *parentPlot);

    void setFrameColor(const QColor& c);
};

#endif // BEAM_COLOR_MAP_H
