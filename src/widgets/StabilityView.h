#ifndef STABILITY_VIEW_H
#define STABILITY_VIEW_H

#include "cameras/CameraTypes.h"
#include "widgets/PlotHelpers.h"

#include <QWidget>

class QCPGraph;
class QCustomPlot;

class StabilityView : public QWidget
{
    Q_OBJECT

public:
    explicit StabilityView(QWidget *parent = nullptr);

    void restoreState(QSettings *s);
    void setThemeColors(PlotHelpers::Theme theme, bool replot);
    void setConfig(const PixelScale& scale);
    void showResult();
    void cleanResult();

private:
    QCustomPlot *_plotTime, *_plotHeat;
    QCPGraph *_timelineX, *_timelineY;
    PixelScale _scale;
};

#endif // STABILITY_VIEW_H
