#include "Plot.h"

#include "plot/BeamGraphIntf.h"

#include "qcustomplot.h"

struct Beam
{
    int w, h;
    uint8_t p;
    QVector<uint8_t> data;
};

Beam renderDemoBeam()
{
    Beam b;
    b.w = 64;
    b.h = 64;
    b.p = 255;
    b.data = QVector<uint8_t>(b.w * b.h);
    auto d = b.data.data();
    const double w2 = 10*10;
    const double ellips = 1;
    for (int i = 0; i < b.h; i++) {
        const double y = (i - b.h/2.0) / ellips;
        for (int j = 0; j < b.w; j++) {
            const double x = j - b.w/2.0;
            d[i*b.w + j] = b.p * exp(-2.0 * (x*x + y*y)/w2);
        }
    }
    return b;
}

void setDefaultAxisFormat(QCPAxis *axis)
{
    axis->setTickLengthIn(0);
    axis->setTickLengthOut(6);
    axis->setSubTickLengthIn(0);
    axis->setSubTickLengthOut(3);
}

Plot::Plot(QWidget *parent) : QWidget{parent}
{
    QMap<double, QColor> rainbowColors {
        { 0.0, QColor(0x2b053e) },
        { 0.1, QColor(0xc2077c) },
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

    _plot = new QCustomPlot;

    // Have to add
    // target_link_libraries: opengl32 and Qt::OpenGL
    // target_compile_definitions: QCUSTOMPLOT_USE_OPENGL
    // But it crashes...
    //_plot->setOpenGl(true, 0);

    _plot->yAxis->setRangeReversed(true);
    setDefaultAxisFormat(_plot->xAxis);
    setDefaultAxisFormat(_plot->yAxis);
    setDefaultAxisFormat(_plot->xAxis2);
    setDefaultAxisFormat(_plot->yAxis2);
    _plot->axisRect()->setupFullAxesBox(true);
    _plot->xAxis->setTickLabels(false);
    _plot->xAxis2->setTickLabels(true);
    _plot->axisRect()->setBackground(rainbowColors[0]);

    auto gridLayer = _plot->xAxis->grid()->layer();
    _plot->addLayer("beam", gridLayer, QCustomPlot::limBelow);

    _colorScale = new QCPColorScale(_plot);
    _colorScale->setBarWidth(10);
    _colorScale->axis()->setPadding(10);
    _plot->plotLayout()->addElement(0, 1, _colorScale);

    _graph = new QCPColorMap(_plot->xAxis, _plot->yAxis);
    _graph->setColorScale(_colorScale);
    _graph->setInterpolate(false);
    QCPColorGradient rainbow;
    rainbow.setColorStops(rainbowColors);
    _graph->setGradient(rainbow);
    _graph->setLayer("beam");

    // Make sure the axis rect and color scale synchronize their bottom and top margins:
    QCPMarginGroup *marginGroup = new QCPMarginGroup(_plot);
    _plot->axisRect()->setMarginGroup(QCP::msBottom|QCP::msTop, marginGroup);
    _colorScale->setMarginGroup(QCP::msBottom|QCP::msTop, marginGroup);

    auto b = renderDemoBeam();
    _imageW = b.w;
    _imageH = b.h;
    auto data = _graph->data();
    data->setSize(b.w, b.h);
    data->setRange(QCPRange(0, b.w), QCPRange(0, b.h));
    for (int y = 0; y < b.h; y++)
        for (int x = 0; x < b.w; x++)
            data->setCell(x, y, b.data.at(y * b.w + x));
    _colorScale->setDataRange(QCPRange(0, b.p));

    auto l = new QVBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    l->addWidget(_plot);

    recalcLimits(true);
}

void Plot::prepare()
{
    _imageW = _graph->data()->keyRange().upper;
    _imageH = _graph->data()->valueRange().upper;
    recalcLimits(false);
}

void Plot::resizeEvent(QResizeEvent *event)
{
    recalcLimits(false);
}

void Plot::recalcLimits(bool replot)
{
    const double plotW = _plot->axisRect()->width();
    const double plotH = _plot->axisRect()->height();
    const double imgW = _imageW;
    const double imgH = _imageH;
    double newImgW = imgW;
    double pixelScale = plotW / imgW;
    double newImgH = plotH / pixelScale;
    if (newImgH < imgH) {
        newImgH = imgH;
        pixelScale = plotH / imgH;
        newImgW = plotW / pixelScale;
    }
    _plot->xAxis->setRange(0, newImgW);
    _plot->yAxis->setRange(0, newImgH);
    if (replot)
        _plot->replot();
}

void Plot::replot()
{
    _plot->replot();
}

QSharedPointer<BeamGraphIntf> Plot::graphIntf() const
{
    return QSharedPointer<BeamGraphIntf>(new BeamGraphIntf(_graph));
}
