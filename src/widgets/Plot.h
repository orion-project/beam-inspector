#ifndef PLOT_H
#define PLOT_H

#include <QWidget>

class QCustomPlot;
class QCPColorMap;
class QCPColorScale;
class QCPItemStraightLine;

class BeamGraphIntf;
class BeamEllipse;

class Plot : public QWidget
{
    Q_OBJECT

public:
    explicit Plot(QWidget *parent = nullptr);

    QSharedPointer<BeamGraphIntf> graphIntf() const;

    void prepare();
    void replot();

protected:
    void resizeEvent(QResizeEvent*) override;

private:
    QCustomPlot *_plot;
    QCPColorMap *_colorMap;
    QCPColorScale *_colorScale;
    QCPItemStraightLine *_lineX, *_lineY;
    BeamEllipse *_beamShape;
    int _imageW, _imageH;

    void renderDemoBeam();
    void recalcLimits(bool replot);
};

#endif // PLOT_H
