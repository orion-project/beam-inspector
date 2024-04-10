#include "BeamGraph.h"

#include "qcp/src/core.h"
#include "qcp/src/painter.h"

//------------------------------------------------------------------------------
//                               BeamColorMapData
//------------------------------------------------------------------------------

BeamColorMapData::BeamColorMapData(int w, int h)
    : QCPColorMapData(w, h, QCPRange(0, w), QCPRange(0, h))
{
}

//------------------------------------------------------------------------------
//                               BeamColorScale
//------------------------------------------------------------------------------

BeamColorScale::BeamColorScale(QCustomPlot *parentPlot) : QCPColorScale(parentPlot)
{
}

void BeamColorScale::setFrameColor(const QColor& c)
{
    for (auto a : mAxisRect->axes())
        a->setBasePen(QPen(c, 0, Qt::SolidLine));
}

//------------------------------------------------------------------------------
//                                BeamEllipse
//------------------------------------------------------------------------------

BeamEllipse::BeamEllipse(QCustomPlot *parentPlot) : QCPAbstractItem(parentPlot)
{
}

void BeamEllipse::draw(QCPPainter *painter)
{
    const double x = parentPlot()->xAxis->coordToPixel(xc);
    const double y = parentPlot()->yAxis->coordToPixel(yc);
    const double rx = parentPlot()->xAxis->coordToPixel(xc + dx/2.0) - x;
    const double ry = parentPlot()->yAxis->coordToPixel(yc + dy/2.0) - y;
    auto t = painter->transform();
    painter->translate(x, y);
    painter->rotate(phi);
    painter->setPen(pen);
    painter->drawEllipse(QPointF(), rx, ry);
    painter->setTransform(t);
}

double BeamEllipse::selectTest(const QPointF &pos, bool onlySelectable, QVariant *details) const
{
    return 0;
}

//------------------------------------------------------------------------------
//                               ApertureRect
//------------------------------------------------------------------------------

ApertureRect::ApertureRect(QCustomPlot *parentPlot) : QCPAbstractItem(parentPlot)
{
    _pen = QPen(Qt::yellow, 0, Qt::DashLine);
    _editPen = QPen(Qt::yellow, 3, Qt::SolidLine);

    connect(parentPlot, &QCustomPlot::mouseMove, this, &ApertureRect::mouseMove);
    connect(parentPlot, &QCustomPlot::mousePress, this, &ApertureRect::mousePress);
    connect(parentPlot, &QCustomPlot::mouseRelease, this, &ApertureRect::mouseRelease);
    connect(parentPlot, &QCustomPlot::mouseDoubleClick, this, &ApertureRect::mouseDoubleClick);
}

double ApertureRect::selectTest(const QPointF &pos, bool onlySelectable, QVariant *details) const
{
    return 0;
}

void ApertureRect::setAperture(const SoftAperture &aperture, bool replot)
{
    _aperture = aperture;
    _x1 = _aperture.x1;
    _y1 = _aperture.y1;
    _x2 = _aperture.x2;
    _y2 = _aperture.y2;
    if (replot)
        parentPlot()->replot();
}

void ApertureRect::startEdit()
{
    if (_editing)
        return;
    _editing = true;
    _x1 = _aperture.x1;
    _y1 = _aperture.y1;
    _x2 = _aperture.x2;
    _y2 = _aperture.y2;
    parentPlot()->replot();
}

void ApertureRect::stopEdit(bool apply)
{
    if (!_editing)
        return;
    _editing = false;
    if (apply) {
        _aperture.x1 = _x1;
        _aperture.y1 = _y1;
        _aperture.x2 = _x2;
        _aperture.y2 = _y2;
    } else {
        _x1 = _aperture.x1;
        _y1 = _aperture.y1;
        _x2 = _aperture.x2;
        _y2 = _aperture.y2;
    }
    parentPlot()->replot();
}

static const int dragThreshold = 10;

void ApertureRect::draw(QCPPainter *painter)
{
    const double x1 = parentPlot()->xAxis->coordToPixel(_x1);
    const double y1 = parentPlot()->yAxis->coordToPixel(_y1);
    const double x2 = parentPlot()->xAxis->coordToPixel(_x2);
    const double y2 = parentPlot()->yAxis->coordToPixel(_y2);
    painter->setPen(_editing ? _editPen : _pen);
    painter->drawRect(x1, y1, x2-x1, y2-y1);
}

void ApertureRect::mouseMove(QMouseEvent *e)
{
    if (!visible() || !_editing) return;

    const double x = e->pos().x();
    const double y = e->pos().y();
    const double t = dragThreshold;

    auto r = parentPlot()->axisRect()->rect();
    if (x < r.left()+t || x >= r.right()-t || y < r.top()+t || y >= r.bottom()-t) {
        _dragging = false;
        showDragCursor(Qt::ArrowCursor);
        return;
    }

    const double x1 = parentPlot()->xAxis->coordToPixel(_x1);
    const double y1 = parentPlot()->yAxis->coordToPixel(_y1);
    const double x2 = parentPlot()->xAxis->coordToPixel(_x2);
    const double y2 = parentPlot()->yAxis->coordToPixel(_y2);

    if (_dragging) {
        const int dx = x - _dragX;
        const int dy = y - _dragY;
        _dragX = x;
        _dragY = y;
        if (_drag0 || _dragX1)
            _x1 = parentPlot()->xAxis->pixelToCoord(x1 + dx);
        if (_drag0 || _dragX2)
            _x2 = parentPlot()->xAxis->pixelToCoord(x2 + dx);
        if (_drag0 || _dragY1)
            _y1 = parentPlot()->yAxis->pixelToCoord(y1 + dy);
        if (_drag0 || _dragY2)
            _y2 = parentPlot()->yAxis->pixelToCoord(y2 + dy);
        parentPlot()->replot();
        return;
    }

    _drag0 = x > x1+t && x < x2-t && y > y1+t && y < y2-t;
    _dragX1 = qAbs(x1-x) < t && y >= (y1-t) && y <= (y2+t);
    _dragX2 = qAbs(x2-x) < t && y >= (y1-t) && y <= (y2+t);
    _dragY1 = qAbs(y1-y) < t && x >= (x1-t) && x <= (x2+t);
    _dragY2 = qAbs(y2-y) < t && x >= (x1-t) && x <= (x2+t);

    if (_drag0)
        showDragCursor(Qt::SizeAllCursor);
    else if ((_dragX1 && _dragY1) || (_dragX2 && _dragY2))
        showDragCursor(Qt::SizeFDiagCursor);
    else if ((_dragX1 && _dragY2) || (_dragX2 && _dragY1))
        showDragCursor(Qt::SizeBDiagCursor);
    else if (_dragX1 || _dragX2)
        showDragCursor(Qt::SizeHorCursor);
    else if (_dragY1 || _dragY2)
        showDragCursor(Qt::SizeVerCursor);
    else
        showDragCursor(Qt::ArrowCursor);
}

void ApertureRect::mousePress(QMouseEvent *e)
{
    if (!visible() || !_editing) return;
    _dragging = true;
    _dragX = e->pos().x();
    _dragY = e->pos().y();
}

void ApertureRect::mouseRelease(QMouseEvent *e)
{
    if (!visible() || !_editing) return;
    _dragging = false;
}

void ApertureRect::mouseDoubleClick(QMouseEvent*)
{
    if (!visible() || !_editing) return;
    stopEdit(true);
}

void ApertureRect::showDragCursor(Qt::CursorShape c)
{
    if (_dragCursor == c)
        return;
    _dragCursor = c;
    QApplication::restoreOverrideCursor();
    if (c != Qt::ArrowCursor)
        QApplication::setOverrideCursor(c);
}
