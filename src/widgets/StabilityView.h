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
    PixelScale _scale;
    double _timelineMinS = -1, _timelineMaxS = -1;
    double _timelineMinV = -1, _timelineMaxV = -1;
    double _offsetVx = -1, _offsetVy = -1;
    qint64 _frameTimeMs = -1;
    qint64 _showTimeMs = -1;
    qint64 _cleanTimeMs = -1;
    qint64 _startTimeMs = -1;
    QVector<DataPoint> _dataBuf;
    int _dataBufCursor = 0;
    double _timelineDisplayS = 300;
    bool _active = false;
    bool _turnedOn = false;
    
    void resetScale(bool time, bool value);
    double timelineDisplayMinS() const;
    void copyGraph(QCPGraph *graph);
};

#endif // STABILITY_VIEW_H
