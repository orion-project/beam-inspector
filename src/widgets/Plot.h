#ifndef PLOT_H
#define PLOT_H

#include <QWidget>

class QCustomPlot;
class QCPColorMap;
class QCPItemStraightLine;
class QCPItemText;

class ApertureRect;
class BeamColorScale;
class BeamEllipse;
class PlotIntf;

class Plot : public QWidget
{
    Q_OBJECT

public:
    explicit Plot(QWidget *parent = nullptr);
    ~Plot();

    PlotIntf* plotIntf() { return _plotIntf; }

    void prepare();
    void replot();

    enum Theme { SYSTEM, LIGHT };
    void setThemeColors(Theme theme, bool replot);
    void setRainbowEnabled(bool on, bool replot);
    void setBeamInfoVisible(bool on, bool replot);
    void selectBackgroundColor();
    void recalcLimits(bool replot);
    void exportImageDlg();
    void startEditAperture();
    void stopEditAperture(bool apply);
    bool isApertureEditing() const;

protected:
    void resizeEvent(QResizeEvent*) override;

private:
    QCustomPlot *_plot;
    PlotIntf *_plotIntf;
    QCPColorMap *_colorMap;
    BeamColorScale *_colorScale;
    QCPItemStraightLine *_lineX, *_lineY;
    QCPItemText *_beamInfo;
    BeamEllipse *_beamShape;
    ApertureRect *_aperture;
    int _imageW, _imageH;

    void renderDemoBeam();
};

#endif // PLOT_H
