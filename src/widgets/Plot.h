#ifndef PLOT_WIDGET_H
#define PLOT_WIDGET_H

#include "cameras/CameraTypes.h"

#include <QWidget>

class QCustomPlot;
class QCPColorMap;
class QCPItemStraightLine;

class RoiRectGraph;
class BeamColorScale;
class BeamEllipse;
class BeamInfoText;
class PlotIntf;

class Plot : public QWidget
{
    Q_OBJECT

public:
    explicit Plot(QWidget *parent = nullptr);
    ~Plot();

    PlotIntf* plotIntf() { return _plotIntf; }

    void replot();

    enum Theme { SYSTEM, LIGHT };
    void setThemeColors(Theme theme, bool replot);
    void setRainbowEnabled(bool on, bool replot);
    void setBeamInfoVisible(bool on, bool replot);
    void setImageSize(int sensorW, int sensorH, const PixelScale &scale);
    void setRoi(const RoiRect &a);
    void selectBackColor();
    void exportImageDlg();
    void zoomAuto(bool replot);
    void zoomFull(bool replot);
    void zoomRoi(bool replot);
    void startEditRoi();
    void stopEditRoi(bool apply);
    bool isRoiEditing() const;
    RoiRect roi() const;
    void adjustWidgetSize();

signals:
    void roiEdited();

protected:
    void resizeEvent(QResizeEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    QCustomPlot *_plot;
    PlotIntf *_plotIntf;
    QCPColorMap *_colorMap;
    BeamColorScale *_colorScale;
    QCPItemStraightLine *_lineX, *_lineY;
    BeamInfoText *_beamInfo;
    BeamEllipse *_beamShape;
    RoiRectGraph *_roi;
    int _imageW, _imageH;
    enum AutoZoomMode { ZOOM_NONE, ZOOM_FULL, ZOOM_APERTURE };
    AutoZoomMode _autoZoom = ZOOM_FULL;
    bool _autoZooming = false;

    void zoomToBounds(double x1, double y1, double x2, double y2, bool replot);
    void axisRangeChanged();
};

#endif // PLOT_WIDGET_H
