#ifndef PLOT_H
#define PLOT_H

#include <QWidget>

class QCustomPlot;
class QCPColorMap;
class QCPColorScale;

class Plot : public QWidget
{
    Q_OBJECT

public:
    explicit Plot(QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent*) override;

private:
    QCustomPlot *_plot;
    QCPColorMap *_graph;
    QCPColorScale *_colorScale;
    int _imageW, _imageH;

    void recalcLimits(bool replot);
};

#endif // PLOT_H
