#ifndef PLOT_INTF_H
#define PLOT_INTF_H

#include "beam_calc.h"

#include "cameras/CameraTypes.h"

class BeamColorMapData;
class BeamAxes;
class BeamEllipse;
class BeamInfoText;
class QCustomPlot;
class QCPColorMap;
class QCPColorScale;

/**
 * Provides access to graph data for camera threads
 * without knowledge about specific graph implementation
 * and avoiding necessity to include whole qcustomplot.h into camera modules
 */
class PlotIntf
{
public:
    PlotIntf(QCustomPlot *plot, QCPColorMap *colorMap, QCPColorScale *colorScale, BeamInfoText *beamInfo);

    void setScale(const PixelScale& scale) { _scale = scale; }
    void setResult(const CgnBeamResult& r, double min, double max);
    void setResult(const QList<CgnBeamResult>& r, double min, double max);
    void showResult();
    void cleanResult();
    void setRawView(bool on);

    void initGraph(int w, int h);
    double* rawGraph() const;
    void invalidateGraph() const;

private:
    QCustomPlot *_plot;
    int _w = 0, _h = 0;
    double _min, _max;
    PixelScale _scale;
    QList<CgnBeamResult> _results;
    BeamInfoText *_beamInfo;
    QCPColorMap *_colorMap;
    QCPColorScale *_colorScale;
    BeamColorMapData *_beamData;
    QList<BeamEllipse*> _beamShapes;
    QList<BeamAxes*> _beamAxes;
};

#endif // PLOT_INTF_H
