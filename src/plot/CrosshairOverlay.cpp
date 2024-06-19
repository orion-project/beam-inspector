#include "CrosshairOverlay.h"

#include "qcp/src/core.h"
#include "qcp/src/painter.h"

#define RADIUS 5
#define EXTENT 5
#define TEXT_MARGIN 5

CrosshairsOverlay::CrosshairsOverlay(QCustomPlot *parentPlot) : QCPAbstractItem(parentPlot)
{
    Crosshair c;
    c.pixelX = 500;
    c.pixelY = 200;
    c.label = "42";
    c.color = Qt::yellow;
    c.text = QStaticText(c.label);
    c.text.prepare();
    _crosshairs << c;
{
    Crosshair c;
    c.pixelX = 600;
    c.pixelY = 300;
    c.label = "100";
    c.color = Qt::red;
    c.text = QStaticText(c.label);
    c.text.prepare();
    _crosshairs << c;
}
    connect(parentPlot, &QCustomPlot::mouseMove, this, &CrosshairsOverlay::mouseMove);
    connect(parentPlot, &QCustomPlot::mousePress, this, &CrosshairsOverlay::mousePress);
    connect(parentPlot, &QCustomPlot::mouseRelease, this, &CrosshairsOverlay::mouseRelease);
}

void CrosshairsOverlay::setImageSize(int sensorW, int sensorH, const PixelScale &scale)
{
    _scale = scale;
    _maxX = scale.pixelToUnit(sensorW);
    _maxY = scale.pixelToUnit(sensorH);
    updateCoords();
}

void CrosshairsOverlay::updateCoords()
{
    for (auto& c : _crosshairs) {
        c.unitX = _scale.pixelToUnit(c.pixelX);
        c.unitY = _scale.pixelToUnit(c.pixelY);
    }
}

void CrosshairsOverlay::draw(QCPPainter *painter)
{
    const double r = RADIUS;
    const double w = RADIUS + EXTENT;
    for (const auto& c : _crosshairs) {
        painter->setBrush(Qt::NoBrush);
        painter->setPen(c.color);
        const double x = parentPlot()->xAxis->coordToPixel(c.unitX);
        const double y = parentPlot()->yAxis->coordToPixel(c.unitY);
        painter->drawEllipse(QPointF(x, y), r, r);
        painter->drawLine(QLineF(x, y-w, x, y+w));
        painter->drawLine(QLineF(x-w, y, x+w, y));
        painter->setBrush(c.color);
        painter->drawStaticText(x+w+TEXT_MARGIN, y-c.text.size().height()/2, c.text);
    }
}

void CrosshairsOverlay::mousePress(QMouseEvent *e)
{
    if (!visible() || !_editing || e->button() != Qt::LeftButton) return;

    _draggingIdx = -1;
    _dragX = e->pos().x();
    _dragY = e->pos().y();
    const double r = RADIUS;
    for (int i = 0; i < _crosshairs.size(); i++) {
        const auto& c = _crosshairs.at(i);
        const double x = parentPlot()->xAxis->coordToPixel(c.unitX);
        const double y = parentPlot()->yAxis->coordToPixel(c.unitY);
        if (_dragX >= x-r && _dragX <= x+r && _dragY >= y-r && _dragY <= y+r) {
            _draggingIdx = i;
            break;
        }
    }
}

void CrosshairsOverlay::mouseRelease(QMouseEvent *e)
{
    _draggingIdx = -1;
}

void CrosshairsOverlay::mouseMove(QMouseEvent *e)
{
    if (_draggingIdx < 0) return;

    const int dx = e->pos().x() - _dragX;
    const int dy = e->pos().y() - _dragY;
    _dragX = e->pos().x();
    _dragY = e->pos().y();

    auto& c = _crosshairs[_draggingIdx];
    const double x = parentPlot()->xAxis->coordToPixel(c.unitX);
    const double y = parentPlot()->yAxis->coordToPixel(c.unitY);
    c.unitX = parentPlot()->xAxis->pixelToCoord(x+dx);
    c.unitY = parentPlot()->yAxis->pixelToCoord(y+dy);
    c.pixelX = _scale.unitToPixel(c.unitX);
    c.pixelY = _scale.unitToPixel(c.unitY);
    parentPlot()->replot();
}
