#ifndef STABILITY_VIEW_H
#define STABILITY_VIEW_H

#include "cameras/CameraTypes.h"
#include "widgets/PlotHelpers.h"

#include "beam_calc.h"

#include <QWidget>

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
    void setConfig(const PixelScale& scale);
    void setResult(qint64 time, const QList<CgnBeamResult>& val);
    void showResult();
    void cleanResult();

private:
    struct DataPoint {
        qint64 time;
        double x;
        double y;
    };

    QCustomPlot *_plotTime, *_plotHeat;
    QCPGraph *_timelineX, *_timelineY;
    QCPTextElement *_timelineLen;
    PixelScale _scale;
    double _timelineMinK = -1, _timelineMaxK = -1;
    double _timelineMinV = -1, _timelineMaxV = -1;
    double _offsetX = -1, _offsetY = -1;
    qint64 _frameTime = -1, _showTime = -1, _cleanTime = -1, _startTime = -1;
    QVector<DataPoint> _dataBuf;
    int _dataBufCursor = 0;
    
    void resetScale(bool key, bool value);
    double timelineDisplayMinK() const;
    void copyGraph(QCPGraph *graph);
};

#endif // STABILITY_VIEW_H
