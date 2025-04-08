#ifndef PROFILES_VIEW_H
#define PROFILES_VIEW_H

#include "widgets/PlotHelpers.h"

#include <QWidget>

class PlotIntf;

class QCPAxisRect;
class QCPGraph;
class QCustomPlot;

class ProfilesView : public QWidget
{
    Q_OBJECT

public:
    explicit ProfilesView(PlotIntf *plotIntf);

    void setThemeColors(PlotHelpers::Theme theme, bool replot);

    void showResult();
    void cleanResult();

private:
    PlotIntf *_plotIntf;
    QCustomPlot *_plot;
    QCPAxisRect *_axisRectMinor;
    QCPAxis *_axisMinorX, *_axisMinorY;
    QCPGraph *_graphX, *_graphY;
    double _profileWidth = 2;
    int _pointCount = 100;
    double rangeX = 0;
};

#endif // PROFILES_VIEW_H
