#ifndef PROFILES_VIEW_H
#define PROFILES_VIEW_H

#include "widgets/PlotHelpers.h"

#include <QWidget>

class QCPAxisRect;
class QCPGraph;
class QCustomPlot;

class ProfilesView : public QWidget
{
    Q_OBJECT

public:
    explicit ProfilesView(QWidget *parent = nullptr);

    void setThemeColors(PlotHelpers::Theme theme, bool replot);

private:
    QCustomPlot *_plot;
    QCPAxisRect *_axisRectY;
    QCPGraph *_graphX, *_graphY;
};

#endif // PROFILES_VIEW_H
