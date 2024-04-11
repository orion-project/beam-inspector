#ifndef BEAM_GRAPH_INTF_H
#define BEAM_GRAPH_INTF_H

#include "beam_calc.h"

class BeamColorMapData;
class BeamEllipse;
class QCPColorMap;
class QCPColorScale;
class QCPItemStraightLine;
class QCPItemText;

/**
 * Provides access to graph data for camera threads
 * without knowledge about specific graph implementation
 * and avoiding necessity to include whole qcustomplot.h into camera modules
 */
class BeamGraphIntf
{
public:
    BeamGraphIntf(
        QCPColorMap *colorMap, QCPColorScale *colorScale,
        BeamEllipse *beamShape, QCPItemText *beamInfo,
        QCPItemStraightLine *lineX, QCPItemStraightLine *lineY);

    void init(int w, int h);
    double* rawData() const;
    void setResult(const CgnBeamResult& r, double min, double max);
    void refreshResult();
    void invalidate() const;

private:
    CgnBeamResult _res;
    QCPItemText *_beamInfo;
    QCPColorMap *_colorMap;
    QCPColorScale *_colorScale;
    BeamColorMapData *_beamData;
    BeamEllipse *_beamShape;
    QCPItemStraightLine *_lineX, *_lineY;
};

#endif // BEAM_GRAPH_INTF_H
