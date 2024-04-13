#ifndef PLOT_WIDGET_H
#define PLOT_WIDGET_H

#include "cameras/CameraTypes.h"

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

    void replot();

    enum Theme { SYSTEM, LIGHT };
    void setThemeColors(Theme theme, bool replot);
    void setRainbowEnabled(bool on, bool replot);
    void setBeamInfoVisible(bool on, bool replot);
    void setImageSize(int sensorW, int sensorH, const PixelScale &scale);
    void setAperture(const SoftAperture &a);
    void selectBackgroundColor();
    void exportImageDlg();
    void zoomAuto(bool replot);
    void zoomFull(bool replot);
    void zoomAperture(bool replot);
    void startEditAperture();
    void stopEditAperture(bool apply);
    bool isApertureEditing() const;
    SoftAperture aperture() const;

signals:
    void apertureEdited();

protected:
    void resizeEvent(QResizeEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

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
    enum AutoZoomMode { ZOOM_NONE, ZOOM_FULL, ZOOM_APERTURE };
    AutoZoomMode _autoZoom = ZOOM_FULL;
    bool _autoZooming = false;

    void zoomToBounds(double x1, double y1, double x2, double y2, bool replot);
    void axisRangeChanged();
};

#endif // PLOT_WIDGET_H
