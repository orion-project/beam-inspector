#ifndef BEAM_GRAPH_INTF_H
#define BEAM_GRAPH_INTF_H

#include "beam_calc.h"

class BeamColorMapData;
class BeamEllipse;
class QCPColorMap;
class QCPColorScale;
class QCPItemStraightLine;

/**
 * Provides access to graph data for camera threads
 * without knowledge about specific graph implementation
 * and avoiding necessity to include whole qcustomplot.h into camera modules
 */
class BeamGraphIntf
{
public:
    BeamGraphIntf(QCPColorMap *colorMap, QCPColorScale *colorScale,
        BeamEllipse *shape, QCPItemStraightLine *lineX, QCPItemStraightLine *lineY);

    void init(int w, int h);
    double* rawData() const;
    void setResult(const CgnBeamResult& r, double min, double max);
    void invalidate() const;

private:
    QCPColorMap *_colorMap;
    QCPColorScale *_colorScale;
    BeamColorMapData *_beamData;
    BeamEllipse *_shape;
    QCPItemStraightLine *_lineX, *_lineY;
};

#endif // BEAM_GRAPH_INTF_H
