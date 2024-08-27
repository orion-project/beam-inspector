#include "Plot.h"

#include "plot/BeamGraph.h"
#include "plot/CrosshairOverlay.h"
#include "plot/PlotExport.h"
#include "plot/RoiRectGraph.h"
#include "widgets/PlotIntf.h"

#include "helpers/OriDialogs.h"
#include "tools/OriSettings.h"
#include "tools/OriMruList.h"
#include "widgets/OriPopupMessage.h"

#include "qcp/src/core.h"
#include "qcp/src/layoutelements/layoutelement-axisrect.h"
#include "qcp/src/items/item-straightline.h"

#define APERTURE_ZOOM_MARGIN 0.01

using Ori::Gui::PopupMessage;

static QColor themeAxisColor(Plot::Theme theme)
{
    bool dark = theme == Plot::SYSTEM && qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    return qApp->palette().color(dark ? QPalette::Light : QPalette::Shadow);
}

static void setDefaultAxisFormat(QCPAxis *axis, Plot::Theme theme)
{
    axis->setTickLengthIn(0);
    axis->setTickLengthOut(6);
    axis->setSubTickLengthIn(0);
    axis->setSubTickLengthOut(3);
    axis->grid()->setPen(QPen(QColor(100, 100, 100), 0, Qt::DotLine));
    axis->setTickLabelColor(theme == Plot::LIGHT ? Qt::black : qApp->palette().color(QPalette::WindowText));
    auto pen = QPen(themeAxisColor(theme), 0, Qt::SolidLine);
    axis->setTickPen(pen);
    axis->setSubTickPen(pen);
    axis->setBasePen(pen);
}

Plot::Plot(QWidget *parent) : QWidget{parent}
{
    _plot = new QCustomPlot;
    _plot->yAxis->setRangeReversed(true);
    _plot->axisRect()->setupFullAxesBox(true);
    _plot->xAxis->setTickLabels(false);
    _plot->xAxis2->setTickLabels(true);
    _plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    //connect(_plot->xAxis, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged), this, &Plot::axisRangeChanged);
    //connect(_plot->yAxis, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged), this, &Plot::axisRangeChanged);
    _plot->axisRect()->setBackground(QBrush(QIcon(":/misc/no_plot").pixmap(16)));
    _plot->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(_plot, &QWidget::customContextMenuRequested, this, &Plot::showContextMenu);

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

    _beamShape = new BeamEllipse(_plot);
    _beamShape->pen = QPen(Qt::white);

    _lineX = new QCPItemStraightLine(_plot);
    _lineY = new QCPItemStraightLine(_plot);
    _lineX->setPen(QPen(Qt::yellow));
    _lineY->setPen(QPen(Qt::white));

    _beamInfo = new BeamInfoText(_plot);

    _roi = new RoiRectGraph(_plot);
    _roi->onEdited = [this]{ emit roiEdited(); };

    setThemeColors(SYSTEM, false);

    auto l = new QVBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    l->addWidget(_plot);

    setCursor(Qt::ArrowCursor);

    _crosshairs = new CrosshairsOverlay(_plot);

    _plotIntf = new PlotIntf(_colorMap, _colorScale, _beamShape, _beamInfo, _lineX, _lineY);

    _mruCrosshairs = new Ori::MruFileList(this);
    _mruCrosshairs->load("mruCrosshairs");
    connect(_mruCrosshairs, &Ori::MruFileList::clicked, this, qOverload<const QString&>(&Plot::loadCrosshairs));
}

Plot::~Plot()
{
    delete _plotIntf;
}

void Plot::setImageSize(int sensorW, int sensorH, const PixelScale &scale)
{
    _imageW = scale.pixelToUnit(sensorW);
    _imageH = scale.pixelToUnit(sensorH);
    _roi->setImageSize(sensorW, sensorH, scale);
    _crosshairs->setImageSize(sensorW, sensorH, scale);
}

void Plot::adjustWidgetSize()
{
    zoomAuto(true);
    _roi->adjustEditorPosition();
}

void Plot::resizeEvent(QResizeEvent*)
{
    // resizeEvent is not called when window gets maximized or restored
    // these cases should be processed separately in the parent window
    adjustWidgetSize();
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

void Plot::zoomToBounds(double x1, double y1, double x2, double y2, bool replot)
{
    const double xx1 = x1;
    const double xx2 = x2;
    const double yy1 = y1;
    const double yy2 = y2;
    const double plotW = _plot->axisRect()->width();
    const double plotH = _plot->axisRect()->height();
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
    _autoZooming = true;
    _plot->xAxis->setRange(xx1, xx1 + newImgW);
    _plot->yAxis->setRange(yy1, yy1 + newImgH);
    _autoZooming = false;
    if (replot)
        _plot->replot();
}

void Plot::zoomFull(bool replot)
{
    _autoZoom = ZOOM_FULL;
    zoomToBounds(0, 0, _imageW, _imageH, replot);
}

void Plot::zoomRoi(bool replot)
{
    if (!_roi->visible()) {
        zoomFull(replot);
        return;
    }
    _autoZoom = ZOOM_APERTURE;
    const double x1 = _roi->getX1();
    const double y1 = _roi->getY1();
    const double x2 = _roi->getX2();
    const double y2 = _roi->getY2();
    const double dx = (x2 - x1) * APERTURE_ZOOM_MARGIN;
    const double dy = (y2 - y1) * APERTURE_ZOOM_MARGIN;
    zoomToBounds(x1-dx, y1-dy, x2+dx, y2+dy, replot);
}

void Plot::zoomAuto(bool replot)
{
    if (_autoZoom == ZOOM_FULL)
        zoomFull(replot);
    else if (_autoZoom == ZOOM_APERTURE)
        zoomRoi(replot);
}

void Plot::axisRangeChanged()
{
    //TODO: Keep user set scale somehow
    //if (!_autoZooming) _autoZoom = ZOOM_NONE;
}

void Plot::replot()
{
    _plot->replot();
}

void Plot::setThemeColors(Theme theme, bool replot)
{
    _plot->setBackground(theme == LIGHT ? QBrush(Qt::white) : palette().brush(QPalette::Base));
    setDefaultAxisFormat(_plot->xAxis, theme);
    setDefaultAxisFormat(_plot->yAxis, theme);
    setDefaultAxisFormat(_plot->xAxis2, theme);
    setDefaultAxisFormat(_plot->yAxis2, theme);
    setDefaultAxisFormat(_colorScale->axis(), theme);
    _colorScale->setFrameColor(themeAxisColor(theme));
    if (replot) _plot->replot();
}

void Plot::setColorMap(const QString& fileName, bool replot)
{
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
    ::exportImageDlg(_plot, [this]{setThemeColors(LIGHT, false);}, [this]{setThemeColors(SYSTEM, false);});
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

void Plot::setRoi(const RoiRect& a)
{
    if (_autoZoom == ZOOM_APERTURE && !a.on)
        _autoZoom = ZOOM_FULL;
    _roi->setRoi(a);
}

RoiRect Plot::roi() const
{
    return _roi->roi();
}

void Plot::setRawView(bool on, bool replot)
{
    _rawView = on;
    setBeamInfoVisible(_showBeamInfo, false);
    _roi->setIsVisible(!_rawView);
    _lineX->setVisible(!_rawView);
    _lineY->setVisible(!_rawView);
    _beamShape->setVisible(!_rawView);
    if (replot) _plot->replot();
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
