#ifndef PLOT_H
#define PLOT_H

#include <QWidget>

class QCustomPlot;
class QCPColorMap;
class QCPItemStraightLine;
class QCPItemText;

class BeamColorScale;
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

    enum Theme { SYSTEM, LIGHT };
    void setThemeColors(Theme theme, bool replot);
    void setRainbowEnabled(bool on, bool replot);
    void setBeamInfoVisible(bool on, bool replot);
    void selectBackgroundColor();
    void recalcLimits(bool replot);
    void exportImageDlg();

protected:
    void resizeEvent(QResizeEvent*) override;

private:
    QCustomPlot *_plot;
    QCPColorMap *_colorMap;
    BeamColorScale *_colorScale;
    QCPItemStraightLine *_lineX, *_lineY;
    QCPItemText *_beamInfo;
    BeamEllipse *_beamShape;
    int _imageW, _imageH;

    void renderDemoBeam();
};

#endif // PLOT_H
