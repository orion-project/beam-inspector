#ifndef PLOT_INTF_H
#define PLOT_INTF_H

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
class PlotIntf
{
public:
    PlotIntf(
        QCPColorMap *colorMap, QCPColorScale *colorScale,
        BeamEllipse *beamShape, QCPItemText *beamInfo,
        QCPItemStraightLine *lineX, QCPItemStraightLine *lineY);

    void cleanResult();
    void setResult(const CgnBeamResult& r, double min, double max);
    void showResult();

    void initGraph(int w, int h);
    double* rawGraph() const;
    void invalidateGraph() const;

private:
    CgnBeamResult _res;
    QCPItemText *_beamInfo;
    QCPColorMap *_colorMap;
    QCPColorScale *_colorScale;
    BeamColorMapData *_beamData;
    BeamEllipse *_beamShape;
    QCPItemStraightLine *_lineX, *_lineY;
    double _min, _max;
};

#endif // PLOT_INTF_H
