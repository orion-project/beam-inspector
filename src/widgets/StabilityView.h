#ifndef STABILITY_VIEW_H
#define STABILITY_VIEW_H

#include "cameras/CameraTypes.h"
#include "widgets/PlotHelpers.h"

#include "beam_calc.h"

#include <QWidget>

class BeamColorScale;
class Heatmap;
class HeatmapData;

class QCPGraph;
class QCPTextElement;
class QCustomPlot;

class StabilityView : public QWidget
{
    Q_OBJECT

public:
    explicit StabilityView(QWidget *parent = nullptr);

    void restoreState(QSettings *s);
    void setThemeColors(PlotHelpers::Theme theme, bool replot);
    void setConfig(const PixelScale& scale, const Stability &stabil);
    void setResult(qint64 time, const QList<CgnBeamResult>& val);
    void showResult();
    void cleanResult();
    void activate(bool on);
    void turnOn(bool on);
    
protected:
    void resizeEvent(QResizeEvent *event);

private:
    struct DataPoint {
        qint64 ms;
        double x;
        double y;
    };

    QCustomPlot *_plotTime, *_plotHeat;
    QCPGraph *_timelineX, *_timelineY;
    QCPTextElement *_timelineDurationText;
    Heatmap *_heatmap;
    HeatmapData *_heatmapData;
    BeamColorScale *_colorScale;
    PixelScale _scale;
    double _timelineMinS, _timelineMaxS;
    double _valueMin, _valueMax;
    double _valueMinX, _valueMaxX;
    double _valueMinY, _valueMaxY;
    double _offsetX = -1, _offsetY = -1;
    qint64 _frameTimeMs = -1;
    qint64 _showTimeMs = -1;
    qint64 _cleanTimeMs = -1;
    qint64 _startTimeMs = -1;
    QVector<DataPoint> _dataBuf;
    int _dataBufCursor = 0;
    double _timelineDisplayS = 300;
    int _heatmapCellCount = 0;
    bool _active = false;
    bool _turnedOn = false;
    
    void resetScale(bool time, bool value);
    double timelineDisplayMinS() const;
    void copyGraph(QCPGraph *graph = nullptr);
    void recalcHeatmap();
    void updateHeatmap(int lastPoints);
};

#endif // STABILITY_VIEW_H
