#ifndef PLOT_WIDGET_H
#define PLOT_WIDGET_H

#include "cameras/CameraTypes.h"
#include "widgets/PlotHelpers.h"

#include <QWidget>

class QCustomPlot;
class QCPItemLine;

class CrosshairsOverlay;
class BeamColorMap;
class BeamColorScale;
class BeamInfoText;
class BeamPlotItem;
class OverexposureWarning;
class PlotIntf;
class RoiRectGraph;
class RoiRectsGraph;

namespace Ori {
class MruFileList;
}

class Plot : public QWidget
{
    Q_OBJECT

public:
    explicit Plot(QWidget *parent = nullptr);
    ~Plot();

    PlotIntf* plotIntf() { return _plotIntf; }

    void replot();

    void setThemeColors(PlotHelpers::Theme theme, bool replot);
    void setColorMap(const QString& colorMap, bool replot);
    void setBeamInfoVisible(bool on, bool replot);
    void setImageSize(int sensorW, int sensorH, const PixelScale &scale);
    void setRoi(const RoiRect &a);
    void setRois(const QList<RoiRect> &a);
    void setRoiMode(RoiMode roiMode);
    void setRawView(bool on, bool replot);
    void selectBackColor();

    void exportImageDlg();

    void zoomAuto(bool replot);
    void zoomFull(bool replot);
    void zoomRoi(bool replot);

    void startEditRoi();
    void stopEditRoi(bool apply);
    bool isRoiEditing() const;
    RoiRect roi() const;
    QList<RoiRect> rois() const;

    bool isCrosshairsVisible() const;
    bool isCrosshairsEditing() const;
    void toggleCrosshairsVisbility();
    void toggleCrosshairsEditing();
    void clearCrosshairs();
    void loadCrosshairs();
    void loadCrosshairs(const QString &fileName);
    void saveCrosshairs();
    Ori::MruFileList *mruCrosshairs() { return _mruCrosshairs; }
    QList<QPointF> crosshairs() const;
    void augmentCrosshairLoadSave(std::function<void(QJsonObject&)> load, std::function<void(QJsonObject&)> save);

    void adjustWidgetSize();

    void storeState(QSettings *s);
    void restoreState(QSettings *s);

signals:
    void roiEdited();
    void crosshairsEdited();
    void crosshairsLoaded();
    void mousePositionChanged(double x, double y, double c);

protected:
    void resizeEvent(QResizeEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void timerEvent(QTimerEvent *event) override;
    bool event(QEvent *event) override;

private:
    QCustomPlot *_plot;
    PlotIntf *_plotIntf;
    BeamColorMap *_colorMap;
    BeamColorScale *_colorScale;
    BeamInfoText *_beamInfo;
    OverexposureWarning *_overexpWarn;
    RoiRectGraph *_roi;
    RoiRectsGraph *_rois;
    RoiMode _roiMode = ROI_NONE;
    CrosshairsOverlay *_crosshairs;
    Ori::MruFileList *_mruCrosshairs;
    QList<BeamPlotItem*> _relativeItems;
    QCPItemLine *_colorLevelMarker;
    int _imageW = 0, _imageH = 0;
    enum AutoZoomMode { ZOOM_NONE, ZOOM_FULL, ZOOM_APERTURE };
    AutoZoomMode _autoZoom = ZOOM_FULL;
    bool _autoZooming = false;
    bool _showBeamInfo = false;
    bool _rawView = false;
    int _timerId = 0;
    double _mouseX = 0, _mouseY = 0;

    void zoomToBounds(double x1, double y1, double x2, double y2, bool replot);
    void axisRangeChanged();
    void showContextMenu(const QPoint& pos);
    void updateRoiVisibility();
    void handleMouseMove(QMouseEvent *event);
    void showLevelAtMouse();
};

#endif // PLOT_WIDGET_H
