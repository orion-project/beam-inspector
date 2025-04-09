#ifndef PROFILES_VIEW_H
#define PROFILES_VIEW_H

#include "cameras/CameraTypes.h"
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
    void setScale(const PixelScale& scale);
    void showResult();
    void cleanResult();

private:
    PlotIntf *_plotIntf;
    QCustomPlot *_plotX, *_plotY;
    QCPGraph *_graphX, *_graphY;
    PixelScale _scale;
    double _profileWidth = 2;
    int _pointCount = 100;
    double _rangeX = 0;
};

#endif // PROFILES_VIEW_H
