#include "Plot.h"

#include "plot/BeamGraph.h"
#include "plot/PlotExport.h"
#include "widgets/PlotIntf.h"

#include "qcp/src/core.h"
#include "qcp/src/layoutelements/layoutelement-axisrect.h"
#include "qcp/src/items/item-straightline.h"
#include "qcp/src/items/item-text.h"

#define APERTURE_ZOOM_MARGIN 0.01

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

    auto gridLayer = _plot->xAxis->grid()->layer();
    _plot->addLayer("beam", gridLayer, QCustomPlot::limBelow);

    _colorScale = new BeamColorScale(_plot);
    _colorScale->setBarWidth(10);
    _colorScale->axis()->setPadding(10);
    _plot->plotLayout()->addElement(0, 1, _colorScale);

    _colorMap = new QCPColorMap(_plot->xAxis, _plot->yAxis);
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

    _beamInfo = new QCPItemText(_plot);
    _beamInfo->position->setType(QCPItemPosition::ptAxisRectRatio);
    _beamInfo->position->setCoords(0.075, 0.11);
    _beamInfo->setColor(Qt::white);
    _beamInfo->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);

    _aperture = new ApertureRect(_plot);
    _aperture->onEdited = [this]{ emit apertureEdited(); };

    setThemeColors(SYSTEM, false);

    auto l = new QVBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    l->addWidget(_plot);

    setCursor(Qt::ArrowCursor);

    _plotIntf = new PlotIntf(_colorMap, _colorScale, _beamShape, _beamInfo, _lineX, _lineY);
}

Plot::~Plot()
{
    delete _plotIntf;
}

void Plot::prepare()
{
    _imageW = _colorMap->data()->keyRange().upper;
    _imageH = _colorMap->data()->valueRange().upper;
    _aperture->maxX = _imageW;
    _aperture->maxY = _imageH;
    zoomAuto(false);
}

void Plot::resizeEvent(QResizeEvent *event)
{
    zoomAuto(false);
}

void Plot::keyPressEvent(QKeyEvent *event)
{
    if (!_aperture->isEditing())
        return;
    if (event->matches(QKeySequence::Cancel))
        _aperture->stopEdit(false);
    else if (event->key() == Qt::Key_Return)
        _aperture->stopEdit(true);
}

void Plot::zoomToBounds(int x1, int y1, int x2, int y2, bool replot)
{
    const double plotW = _plot->axisRect()->width();
    const double plotH = _plot->axisRect()->height();
    double imgW = x2 - x1;
    double imgH = y2 - y1;
    double newImgW = imgW;
    double pixelScale = plotW / imgW;
    double newImgH = plotH / pixelScale;
    if (newImgH < imgH) {
        newImgH = imgH;
        pixelScale = plotH / imgH;
        newImgW = plotW / pixelScale;
    }
    //qDebug() << "zoom";
    _autoZooming = true;
    _plot->xAxis->setRange(x1, x1 + newImgW);
    _plot->yAxis->setRange(y1, y1 + newImgH);
    _autoZooming = false;
    if (replot)
        _plot->replot();
}

void Plot::zoomFull(bool replot)
{
    _autoZoom = ZOOM_FULL;
    zoomToBounds(0, 0, _imageW, _imageH, replot);
}

void Plot::zoomAperture(bool replot)
{
    if (!_aperture->visible()) {
        zoomFull(replot);
        return;
    }
    _autoZoom = ZOOM_APERTURE;
    auto a = _aperture->aperture();
    int dx = a.width() * APERTURE_ZOOM_MARGIN;
    int dy = a.height() * APERTURE_ZOOM_MARGIN;
    zoomToBounds(a.x1-dx, a.y1-dy, a.x2+dx, a.y2+dy, replot);
}

void Plot::zoomAuto(bool replot)
{
    if (_autoZoom == ZOOM_FULL)
        zoomFull(replot);
    else if (_autoZoom == ZOOM_APERTURE)
        zoomAperture(replot);
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

void Plot::setRainbowEnabled(bool on, bool replot)
{
    if (!on) {
        _colorMap->setGradient(QCPColorGradient::GradientPreset::gpGrayscale);
        _plot->axisRect()->setBackground(QColor(0x2b053e));
        if (replot)
            _plot->replot();
        return;
    }

//    Grad0 in img/gradient.svg
//    QMap<double, QColor> rainbowColors {
//        { 0.0, QColor(0x2b053e) },
//        { 0.1, QColor(0xc2077c) },
//        { 0.15, QColor(0xbe05f3) },
//        { 0.2, QColor(0x2306fb) },
//        { 0.3, QColor(0x0675db) },
//        { 0.4, QColor(0x05f9ee) },
//        { 0.5, QColor(0x04ca04) },
//        { 0.65, QColor(0xfafd05) },
//        { 0.8, QColor(0xfc8705) },
//        { 0.9, QColor(0xfc4d06) },
//        { 1.0, QColor(0xfc5004) },
//    };

//    Grad1 in img/gradient.svg
//    QMap<double, QColor> rainbowColors {
//        { 0.00, QColor(0x2b053e) },
//        { 0.05, QColor(0xc4138a) },
//        { 0.10, QColor(0x9e0666) },
//        { 0.15, QColor(0xbe05f3) },
//        { 0.20, QColor(0x2306fb) },
//        { 0.30, QColor(0x0675db) },
//        { 0.40, QColor(0x05f9ee) },
//        { 0.50, QColor(0x04ca04) },
//        { 0.65, QColor(0xfafd05) },
//        { 0.80, QColor(0xfc8705) },
//        { 0.90, QColor(0xfc4d06) },
//        { 1.00, QColor(0xfc5004) },
//    };

    // Grad2 in img/gradient.svg
    QMap<double, QColor> rainbowColors {
        { 0.0, QColor(0x2b053e) },
        { 0.075, QColor(0xd60c8a) },
        { 0.15, QColor(0xbe05f3) },
        { 0.2, QColor(0x2306fb) },
        { 0.3, QColor(0x0675db) },
        { 0.4, QColor(0x05f9ee) },
        { 0.5, QColor(0x04ca04) },
        { 0.65, QColor(0xfafd05) },
        { 0.8, QColor(0xfc8705) },
        { 0.9, QColor(0xfc4d06) },
        { 1.0, QColor(0xfc5004) },
    };

    QCPColorGradient rainbow;
    rainbow.setColorStops(rainbowColors);
    _colorMap->setGradient(rainbow);
    _plot->axisRect()->setBackground(Qt::black);
    if (replot) _plot->replot();
}

void Plot::setBeamInfoVisible(bool on, bool replot)
{
    _beamInfo->setVisible(on);
    if (on) _plotIntf->showResult();
    if (replot) _plot->replot();
}

void Plot::selectBackgroundColor()
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

void Plot::startEditAperture()
{
    _aperture->startEdit();
}

void Plot::stopEditAperture(bool apply)
{
    _aperture->stopEdit(apply);
}

bool Plot::isApertureEditing() const
{
    return _aperture->isEditing();
}

void Plot::setAperture(const SoftAperture& a, bool replot)
{
    if (_autoZoom == ZOOM_APERTURE && !a.enabled)
        _autoZoom = ZOOM_FULL;
    _aperture->setAperture(a, replot);
}

SoftAperture Plot::aperture() const
{
    return _aperture->aperture();
}
