#ifndef PLOT_INTF_H
#define PLOT_INTF_H

#include "beam_calc.h"

#include "cameras/CameraTypes.h"

class BeamColorMapData;
class BeamEllipse;
class BeamInfoText;
class QCPColorMap;
class QCPColorScale;
class QCPItemStraightLine;

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
        BeamEllipse *beamShape, BeamInfoText *beamInfo,
        QCPItemStraightLine *lineX, QCPItemStraightLine *lineY);

    void setScale(const PixelScale& scale) { _scale = scale; }
    void setResult(const CgnBeamResult& r, double min, double max);
    void showResult();
    void cleanResult();

    void initGraph(int w, int h);
    double* rawGraph() const;
    void invalidateGraph() const;

private:
    int _w = 0, _h = 0;
    double _min, _max;
    PixelScale _scale;
    CgnBeamResult _res;
    BeamInfoText *_beamInfo;
    QCPColorMap *_colorMap;
    QCPColorScale *_colorScale;
    BeamColorMapData *_beamData;
    BeamEllipse *_beamShape;
    QCPItemStraightLine *_lineX, *_lineY;
};

#endif // PLOT_INTF_H
