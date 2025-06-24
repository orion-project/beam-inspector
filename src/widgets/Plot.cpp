#include "Plot.h"

#include "app/AppSettings.h"
#include "plot/BeamGraph.h"
#include "plot/CrosshairOverlay.h"
#include "plot/PlotExport.h"
#include "plot/RoiRectGraph.h"
#include "widgets/PlotHelpers.h"
#include "widgets/PlotIntf.h"

#include "helpers/OriDialogs.h"
#include "tools/OriSettings.h"
#include "tools/OriMruList.h"
#include "widgets/OriPopupMessage.h"

#include "qcustomplot/src/core.h"
#include "qcustomplot/src/items/item-line.h"
#include "qcustomplot/src/layoutelements/layoutelement-axisrect.h"

#define LOG_ID "Plot:"
#define APERTURE_ZOOM_MARGIN 0.01

using Ori::Gui::PopupMessage;

Plot::Plot(QWidget *parent) : QWidget{parent}
{
    _plot = new QCustomPlot;
    _plot->yAxis->ignoreMouseWheel = true;
    _plot->yAxis->setRangeReversed(true);
    _plot->axisRect()->setupFullAxesBox(true);
    _plot->xAxis->ignoreMouseWheel = true;
    _plot->xAxis->setTickLabels(false);
    _plot->xAxis2->setTickLabels(true);
    _plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    _plot->skipRepaintOnResize = true;
    connect(_plot, &QCustomPlot::afterReplot, this, &Plot::afterReplot);
    connect(_plot->xAxis, qOverload<const QCPRange&>(&QCPAxis::rangeChanged), this, &Plot::axisRangeChanged);
    connect(_plot->yAxis, qOverload<const QCPRange&>(&QCPAxis::rangeChanged), this, &Plot::axisRangeChanged);
    _plot->axisRect()->setBackground(QBrush(QIcon(":/misc/no_plot").pixmap(16)));
    _plot->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(_plot, &QWidget::customContextMenuRequested, this, &Plot::showContextMenu);
    connect(_plot, &QCustomPlot::mouseMove, this, &Plot::handleMouseMove);
    
    _axisRect = _plot->axisRect();

    auto gridLayer = _plot->xAxis->grid()->layer();
    _plot->addLayer("beam", gridLayer, QCustomPlot::limBelow);

    _colorScale = new BeamColorScale(_plot);
    _colorScale->setBarWidth(10);
    _colorScale->axis()->setPadding(10);
    _plot->plotLayout()->addElement(0, 1, _colorScale);

    _colorMap = new BeamColorMap(_plot->xAxis, _plot->yAxis);
    _colorMap->setColorScale(_colorScale);
    _colorMap->setInterpolate(false);
    _colorMap->setLayer("beam");

    // Make sure the axis rect and color scale synchronize their bottom and top margins:
    QCPMarginGroup *marginGroup = new QCPMarginGroup(_plot);
    _plot->axisRect()->setMarginGroup(QCP::msBottom|QCP::msTop, marginGroup);
    _colorScale->setMarginGroup(QCP::msBottom|QCP::msTop, marginGroup);

    _beamInfo = new BeamInfoText(_plot);

    _overexpWarn = new OverexposureWarning(_plot);
    _overexpWarn->setVisible(false);

    _roi = new RoiRectGraph(_plot);
    _roi->onEdited = [this]{ emit roiEdited(); };
    _relativeItems << _roi;

    _rois = new RoiRectsGraph(_plot);
    _relativeItems << _rois;

    auto l = new QVBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    l->addWidget(_plot);

    setCursor(Qt::ArrowCursor);

    _crosshairs = new CrosshairsOverlay(_plot);
    _crosshairs->onEdited = [this]{ emit crosshairsEdited(); };
    _relativeItems << _crosshairs;

    _plotIntf = new PlotIntf(this, _plot, _colorMap, _colorScale, _beamInfo, _rois);
    _plotIntf->onDataShown = [this]{ showLevelAtMouse(); };

    _colorLevelMarker = new QCPItemLine(_plot);
    _colorLevelMarker->setLayer("overlay");
    _colorLevelMarker->setClipToAxisRect(false);
    _colorLevelMarker->setHead(QCPLineEnding(QCPLineEnding::esFlatArrow));
    _colorLevelMarker->end->setAxisRect(_colorScale->axisRect());
    _colorLevelMarker->end->setAxes(_colorScale->axisRect()->axis(QCPAxis::atBottom),
                                    _colorScale->axisRect()->axis(QCPAxis::atRight));
    _colorLevelMarker->end->setTypeX(QCPItemPosition::ptAxisRectRatio);
    _colorLevelMarker->end->setTypeY(QCPItemPosition::ptPlotCoords);
    _colorLevelMarker->end->setCoords(0, 0.5);
    _colorLevelMarker->start->setAxisRect(_colorScale->axisRect());
    _colorLevelMarker->start->setTypeX(QCPItemPosition::ptAbsolute);
    _colorLevelMarker->start->setTypeY(QCPItemPosition::ptPlotCoords);
    _colorLevelMarker->start->setParentAnchor(_colorLevelMarker->end);
    _colorLevelMarker->start->setCoords(-_colorLevelMarker->head().length(), 0);

    _mruCrosshairs = new Ori::MruFileList(this);
    _mruCrosshairs->load("mruCrosshairs");
    connect(_mruCrosshairs, &Ori::MruFileList::clicked, this, qOverload<const QString&>(&Plot::loadCrosshairs));

    setThemeColors(PlotHelpers::SYSTEM, false);
}

Plot::~Plot()
{
    delete _plotIntf;
}

void Plot::augmentCrosshairLoadSave(std::function<void(QJsonObject&)> load, std::function<void(QJsonObject&)> save)
{
    _crosshairs->loadMore = load;
    _crosshairs->saveMore = save;
}

void Plot::setImageSize(int sensorW, int sensorH, const PixelScale &scale, bool keepZoom)
{
    _imageW = scale.pixelToUnit(sensorW);
    _imageH = scale.pixelToUnit(sensorH);
    qDebug().nospace() << LOG_ID 
        << " Image size " << sensorW << 'x' << sensorH 
        << ", scaled to " << _imageW << 'x' << _imageH
        << ", viewport " << _axisRect->width() << 'x' << _axisRect->height()
        << ", keepZoom=" << keepZoom;
    for (const auto &it : std::as_const(_relativeItems))
        it->setImageSize(sensorW, sensorH, scale);
    if (keepZoom) {
        if (_autoZoomed)
            zoomAuto(false);
        else {
            adjustRangeForScaleFactor(_rangeX, scale.scaleFactor());
            adjustRangeForScaleFactor(_rangeY, scale.scaleFactor());
        }
    }
    _scaleFactor = scale.scaleFactor();
}

void Plot::adjustPlotForWidgetSize(bool replot)
{
    if (_autoZoomed) {
        zoomAuto(false);
    } else {
        adjustRangeForWidgetSize(_rangeX);
        adjustRangeForWidgetSize(_rangeY);
    }
    _roi->adjustEditorPosition();
    if (replot) _plot->replot();
}

void Plot::afterReplot()
{
    if (_imageW == 0 || _imageH == 0) return;
    
    // During replot the plottable width can change
    // because the plot calculates label widths during replotting
    // If it's changed we have to replot again into the changed rect
    if (_axisRect->width() != _rangeX.widgetSize) {
        adjustPlotForWidgetSize(false);
        _plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void Plot::resizeEvent(QResizeEvent*)
{
    if (_imageW == 0 || _imageH == 0) return;

    // resizeEvent is not called when window gets maximized or restored
    // these cases should be processed separately in the parent window

    // By default QCustomPlot gets replotted and repainted when resized. 
    // Repainting is intentionally supressed, see skipRepaintOnResize.
    // We still need replot() on resize because axisRect() has valid size only after replot()
    // and we need this axis rect size to properly calculate axes ranges.
    // After the axes ranges are recalcuated for the new widget size,
    // the plot needs to be replotted again. Without repaint suppressing such double replotting 
    // is obviously visible and causes flickering when plot is being resized.
    adjustPlotForWidgetSize(true);
}

void Plot::keyPressEvent(QKeyEvent *e)
{
    if (!_roi->isEditing())
        return;
    switch (e->key()) {
    case Qt::Key_Escape:
        _roi->stopEdit(false);
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        _roi->stopEdit(true);
        break;
    }
}

bool Plot::event(QEvent *event)
{
    if (auto e = dynamic_cast<ExpWarningEvent*>(event); e) {
        bool warn = e->overexposed > AppSettings::instance().overexposedPixelsPercent / 100.0;
        if (_overexpWarn->visible() != warn) {
            _overexpWarn->setVisible(warn);
        }
        //qDebug() << "Overexposed" << e->overexposed << warn;
        return true;
    }
    return QWidget::event(event);
}

void Plot::zoomToBounds(double x1, double y1, double x2, double y2, bool replot)
{
    const double xx1 = x1;
    const double xx2 = x2;
    const double yy1 = y1;
    const double yy2 = y2;
    const double plotW = _axisRect->width();
    const double plotH = _axisRect->height();
    const double imgW = xx2 - xx1;
    const double imgH = yy2 - yy1;
    double newImgW = imgW;
    double pixelScale = plotW / imgW;
    double newImgH = plotH / pixelScale;
    if (newImgH < imgH) {
        newImgH = imgH;
        pixelScale = plotH / imgH;
        newImgW = plotW / pixelScale;
    }
    _rangeChanging = true;
    _plot->xAxis->setRange(xx1, xx1 + newImgW);
    _plot->yAxis->setRange(yy1, yy1 + newImgH);
    _rangeChanging = false;
    storeRange(_rangeX);
    storeRange(_rangeY);
    if (replot)
        _plot->replot();
}

void Plot::zoomFull(bool replot)
{
    if (_imageW <= 0 || _imageH <= 0) return;
    _autoZoom = ZOOM_FULL;
    _autoZoomed = true;
    qDebug().nospace() << LOG_ID
        << " Autozoom (full) " << _imageW << 'x' << _imageH
        << ", viewport " << _axisRect->width() << 'x' << _axisRect->height();
    zoomToBounds(0, 0, _imageW, _imageH, replot);
}

void Plot::zoomRoi(bool replot)
{
    if (!_roi->visible()) {
        zoomFull(replot);
        return;
    }
    _autoZoom = ZOOM_APERTURE;
    _autoZoomed = true;
    const double x1 = _roi->getX1();
    const double y1 = _roi->getY1();
    const double x2 = _roi->getX2();
    const double y2 = _roi->getY2();
    const double dx = (x2 - x1) * APERTURE_ZOOM_MARGIN;
    const double dy = (y2 - y1) * APERTURE_ZOOM_MARGIN;
    qDebug().nospace() << LOG_ID
        << " Autozoom (ROI) " << x2-x1 << 'x' << y2-y1
        << ", viewport " << _axisRect->width() << 'x' << _axisRect->height();
    zoomToBounds(x1-dx, y1-dy, x2+dx, y2+dy, replot);
}

void Plot::zoomAuto(bool replot)
{
    if (_autoZoom == ZOOM_APERTURE && _roiMode == ROI_SINGLE)
        zoomRoi(replot);
    else
        zoomFull(replot);
}

void Plot::storeRange(AxisRange &r)
{
    auto range = (&r == &_rangeX ? _plot->xAxis : _plot->yAxis)->range();
    r.min = range.lower;
    r.max = range.upper;
    r.widgetSize = &r == &_rangeX ? _axisRect->width() : _axisRect->height();
}

void Plot::adjustRangeForWidgetSize(AxisRange &r)
{
    if (r.widgetSize == 0) return;
    const int newSize = &r == &_rangeX ? _axisRect->width() : _axisRect->height();
    if (newSize == r.widgetSize) return;
    r.max = r.min + newSize * (r.max - r.min) / double(r.widgetSize);
    r.widgetSize = newSize;
    _rangeChanging = true;
    (&r == &_rangeX ? _plot->xAxis : _plot->yAxis)->setRange(r.min, r.max);
    _rangeChanging = false;
}

void Plot::adjustRangeForScaleFactor(AxisRange &r, double newFactor)
{
    r.min *= newFactor / _scaleFactor;
    r.max *= newFactor / _scaleFactor;
    _rangeChanging = true;
    (&r == &_rangeX ? _plot->xAxis : _plot->yAxis)->setRange(r.min, r.max);
    _rangeChanging = false;
}

void Plot::axisRangeChanged()
{
    if (_rangeChanging) return;
    _autoZoomed = false;
    storeRange(_rangeX);
    storeRange(_rangeY);
}

void Plot::replot()
{
    _plot->replot();
}

void Plot::setThemeColors(PlotHelpers::Theme theme, bool replot)
{
    PlotHelpers::setThemeColors(_plot, theme);
    PlotHelpers::setDefaultAxisFormat(_colorScale->axis(), theme);
    _colorScale->setFrameColor(PlotHelpers::themeAxisColor(theme));
    _colorLevelMarker->setPen(PlotHelpers::themeAxisColor(theme));
    if (replot) _plot->replot();
}

void Plot::setColorMap(const QString& colorMap, bool replot)
{
    QString fileName = AppSettings::colorMapPath(colorMap.isEmpty() ? AppSettings::defaultColorMap() : colorMap);
    PrecalculatedGradient gradient(fileName);
    _colorMap->setGradient(gradient);
    _colorScale->setGradient(gradient);
    if (replot) _plot->replot();
}

void Plot::setBeamInfoVisible(bool on, bool replot)
{
    _showBeamInfo = on;
    _beamInfo->setVisible(_showBeamInfo && !_rawView);
    if (on) _plotIntf->showResult();
    if (replot) _plot->replot();
}

void Plot::selectBackColor()
{
    auto c = QColorDialog::getColor(_plot->axisRect()->backgroundBrush().color(), this);
    if (c.isValid()) {
        _plot->axisRect()->setBackground(c);
        _plot->replot();
    }
}

void Plot::exportImageDlg()
{
    ::exportImageDlg(_plot,
        [this]{setThemeColors(PlotHelpers::LIGHT, false);},
        [this]{setThemeColors(PlotHelpers::SYSTEM, false);});
}

void Plot::startEditRoi()
{
    _roi->startEdit();
}

void Plot::stopEditRoi(bool apply)
{
    _roi->stopEdit(apply);
}

bool Plot::isRoiEditing() const
{
    return _roi->isEditing();
}

void Plot::setRoi(const RoiRect& r)
{
    _roi->setRoi(r);
}

void Plot::setRois(const QList<RoiRect> &r)
{
    _rois->setRois(r, _crosshairs->goodnessLimits(r.size()));
}

void Plot::setRoiMode(RoiMode roiMode)
{
    _roiMode = roiMode;
    updateRoiVisibility();
}

RoiRect Plot::roi() const
{
    return _roi->roi();
}

void Plot::setRawView(bool on, bool replot)
{
    _rawView = on;
    setBeamInfoVisible(_showBeamInfo, false);
    _plotIntf->setRawView(on);
    updateRoiVisibility();
    if (replot) _plot->replot();
}

void Plot::updateRoiVisibility()
{
    _roi->setIsVisible(!_rawView && _roiMode == ROI_SINGLE);
    _rois->setVisible(!_rawView && _roiMode == ROI_MULTI);
}

void Plot::showContextMenu(const QPoint& pos)
{
    if (_crosshairs->visible() && _crosshairs->isEditing())
        _crosshairs->showContextMenu(pos);
}

void Plot::storeState(QSettings *s)
{
    s->beginGroup("Plot");
    s->setValue("crosshairs", _crosshairs->visible());
    s->endGroup();

    _crosshairs->save(s);
}

void Plot::restoreState(QSettings *s)
{
    s->beginGroup("Plot");
    _crosshairs->setVisible(s->value("crosshairs", false).toBool());
    s->endGroup();

    _crosshairs->load(s);
}

bool Plot::isCrosshairsVisible() const
{
    return _crosshairs->visible();
}

bool Plot::isCrosshairsEditing() const
{
    return _crosshairs->isEditing();
}

void Plot::toggleCrosshairsVisbility()
{
    _crosshairs->setVisible(!_crosshairs->visible());
    if (!_crosshairs->visible())
        _crosshairs->setEditing(false);
    if (_crosshairs->visible() && _crosshairs->isEmpty())
        PopupMessage::show({
            .mode = Ori::Gui::PopupMessage::HINT,
            .text = tr("Crosshairs are not defined."
                "<br>To add them use the command:<p>"
                "<code>Overlays -> Edit Crosshairs</code>"),
            .duration = 0,
            .textAlign = Qt::AlignLeft,
            .pixmap = QIcon(":/toolbar/crosshair").pixmap(48)
        }, this);
    replot();
}

void Plot::toggleCrosshairsEditing()
{
    _crosshairs->setEditing(!_crosshairs->isEditing());
    if (_crosshairs->isEditing() && !_crosshairs->visible()) {
        _crosshairs->setVisible(true);
        replot();
    }
    if (_crosshairs->isEditing() && _crosshairs->isEmpty())
        PopupMessage::show({
            .mode = Ori::Gui::PopupMessage::HINT,
            .text = tr("Crosshairs are not yet defined."
                "<p>Use context menu to add them"),
            .textAlign = Qt::AlignLeft,
            .pixmap = QIcon(":/toolbar/crosshair").pixmap(48)
        }, this);
}

void Plot::clearCrosshairs()
{
    if (_crosshairs->isEmpty()) {
        PopupMessage::hint(tr("There are no crosshairs on the plot"));
        return;
    }
    if (Ori::Dlg::yes(tr("Remove all crosshairs?"))) {
        _crosshairs->clear();
        replot();
        emit crosshairsEdited();
    }
}

void Plot::loadCrosshairs()
{
    Ori::Settings s;
    s.beginGroup("Crosshairs");
    auto recentDir = s.value("recentDir").toString();

    QString fileName = QFileDialog::getOpenFileName(qApp->activeWindow(),
        tr("Load Crosshairs"), recentDir, tr("JSON files (*.json);;All files (*.*)"));
    if (fileName.isEmpty())
        return;

    QFileInfo fi(fileName);
    s.setValue("recentDir", fi.absoluteDir().absolutePath());
    fileName = fi.absoluteFilePath();
    loadCrosshairs(fileName);
}

void Plot::loadCrosshairs(const QString &fileName)
{
    auto res = _crosshairs->load(fileName);
    if (!res.isEmpty())
        Ori::Dlg::error(res);
    else
        replot();
    _mruCrosshairs->append(fileName);
    emit crosshairsLoaded();
}

void Plot::saveCrosshairs()
{
    Ori::Settings s;
    s.beginGroup("Crosshairs");
    auto recentDir = s.value("recentDir").toString();

    QString fileName = QFileDialog::getSaveFileName(qApp->activeWindow(),
        tr("Save Crosshairs"), recentDir, tr("JSON files (*.json);;All files (*.*)"));
    if (fileName.isEmpty())
        return;

    QFileInfo fi(fileName);
    s.setValue("recentDir", fi.absoluteDir().absolutePath());
    fileName = fi.absoluteFilePath();

    auto res = _crosshairs->save(fileName);
    if (!res.isEmpty())
        Ori::Dlg::error(res);
    else
        _mruCrosshairs->append(fileName);
}

QList<QPointF> Plot::crosshairs() const
{
    return _crosshairs->positions();
}

QList<RoiRect> Plot::rois() const
{
    return _crosshairs->rois();
}

void Plot::handleMouseMove(QMouseEvent *event)
{
    _mouseX = _plot->xAxis->pixelToCoord(event->pos().x());
    _mouseY = _plot->yAxis->pixelToCoord(event->pos().y());

    if (_timerId == 0)
        _timerId = startTimer(100);
}

void Plot::timerEvent(QTimerEvent *event)
{
    showLevelAtMouse();
    _plot->replot();

    killTimer(_timerId);
    _timerId = 0;
}

void Plot::showLevelAtMouse()
{
    if (_mouseX < 0 || _mouseY < 0) return;

    // Average colors over 0.5% of image size using 3x3 grid
    const double dx = double(_imageW)*0.0025;
    const double dy = double(_imageH)*0.0025;
    double c = 0;
    for (int sy = -1; sy <= 1; sy++)
        for (int sx = -1; sx <= 1; sx++) {
            const double x = _mouseX + double(sx)*dx;
            const double y = _mouseY + double(sy)*dy;
            c += _colorMap->data()->data(x, y);
        }
    c /= 9.0;

    _colorLevelMarker->end->setCoords(0, c);
    emit mousePositionChanged(_mouseX, _mouseY, c);
}