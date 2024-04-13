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
    setRangeDrag(false);
    setRangeZoom(false);
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

void ApertureRect::setAperture(const SoftAperture &aperture)
{
    _aperture = aperture;
    updateCoords();
    setVisible(_aperture.on);
}

void ApertureRect::setImageSize(int sensorW, int sensorH, const PixelScale &scale)
{
    _scale = scale;
    _maxX = scale.sensorToUnit(sensorW);
    _maxY = scale.sensorToUnit(sensorH);
    updateCoords();
}

void ApertureRect::updateCoords()
{
    _x1 = _scale.sensorToUnit(_aperture.x1);
    _y1 = _scale.sensorToUnit(_aperture.y1);
    _x2 = _scale.sensorToUnit(_aperture.x2);
    _y2 = _scale.sensorToUnit(_aperture.y2);
}

void ApertureRect::startEdit()
{
    if (_editing)
        return;
    _editing = true;
    const double sensorW = _scale.unitToSensor(_maxX);
    const double sensorH = _scale.unitToSensor(_maxY);
    if (!_aperture.isValid(sensorW, sensorH)) {
        _x1 = _maxX / 4;
        _x2 = _maxX / 4 * 3;
        _y1 = _maxY / 4;
        _y2 = _maxY / 4 * 3;
    }
    setVisible(true);
    parentPlot()->replot();
}

void ApertureRect::stopEdit(bool apply)
{
    if (!_editing)
        return;
    _editing = false;
    if (apply) {
        _aperture.x1 = _scale.unitToSensor(_x1);
        _aperture.y1 = _scale.unitToSensor(_y1);
        _aperture.x2 = _scale.unitToSensor(_x2);
        _aperture.y2 = _scale.unitToSensor(_y2);
        _aperture.on = true;
        if (onEdited)
            onEdited();
    } else {
        updateCoords();
        setVisible(_aperture.on);
    }
    QToolTip::hideText();
    resetDragCusrsor();
    parentPlot()->replot();
}

static const int dragThreshold = 10;

void ApertureRect::draw(QCPPainter *painter)
{
    const double x1 = parentPlot()->xAxis->coordToPixel(_x1);
    const double y1 = parentPlot()->yAxis->coordToPixel(_y1);
    const double x2 = parentPlot()->xAxis->coordToPixel(_x2);
    const double y2 = parentPlot()->yAxis->coordToPixel(_y2);
    if (_editing) {
        auto r = parentPlot()->axisRect()->rect();
        painter->setPen(_pen);
        painter->drawLine(QLineF(r.left(), y1, r.right(), y1));
        painter->drawLine(QLineF(r.left(), y2, r.right(), y2));
        painter->drawLine(QLineF(x1, r.top(), x1, r.bottom()));
        painter->drawLine(QLineF(x2, r.top(), x2, r.bottom()));
        painter->setPen(_editPen);
        painter->drawRect(x1, y1, x2-x1, y2-y1);
    } else {
        painter->setPen(_pen);
        painter->drawRect(x1, y1, x2-x1, y2-y1);
    }
//    painter->setPen(_editing ? _editPen : _pen);
//    painter->drawRect(x1, y1, x2-x1, y2-y1);
}

void ApertureRect::mouseMove(QMouseEvent *e)
{
    if (!visible() || !_editing) return;

    if (!parentPlot()->axisRect()->rect().contains(e->pos())) {
        resetDragCusrsor();
        _dragging = false;
        return;
    }

    const double x = e->pos().x();
    const double y = e->pos().y();
    const double t = dragThreshold;
    const double x1 = parentPlot()->xAxis->coordToPixel(_x1);
    const double y1 = parentPlot()->yAxis->coordToPixel(_y1);
    const double x2 = parentPlot()->xAxis->coordToPixel(_x2);
    const double y2 = parentPlot()->yAxis->coordToPixel(_y2);

    if (_dragging) {
        const int dx = x - _dragX;
        const int dy = y - _dragY;
        _dragX = x;
        _dragY = y;
        if (_drag0 || _dragX1) {
            _x1 = parentPlot()->xAxis->pixelToCoord(qMin(x1+dx, x2-t));
            _x1 = qMax(_x1, 0.0);
        }
        if (_drag0 || _dragX2) {
            _x2 = parentPlot()->xAxis->pixelToCoord(qMax(x2+dx, x1+t));
            _x2 = qMin(_x2, _maxX);
        }
        if (_drag0 || _dragY1) {
            _y1 = parentPlot()->yAxis->pixelToCoord(qMin(y1+dy, y2-t));
            _y1 = qMax(_y1, 0.0);
        }
        if (_drag0 || _dragY2) {
            _y2 = parentPlot()->yAxis->pixelToCoord(qMax(y2+dy, y1+t));
            _y2 = qMin(_y2, _maxY);
        }
        showCoordTooltip(e->globalPosition().toPoint());
        parentPlot()->replot();
        return;
    }

    _drag0 = x > x1+t && x < x2-t && y > y1+t && y < y2-t;
    _dragX1 = !_drag0 && qAbs(x1-x) < t;
    _dragX2 = !_drag0 && qAbs(x2-x) < t;
    _dragY1 = !_drag0 && qAbs(y1-y) < t;
    _dragY2 = !_drag0 && qAbs(y2-y) < t;
//    _dragX1 = qAbs(x1-x) < t && y >= (y1-t) && y <= (y2+t);
//    _dragX2 = qAbs(x2-x) < t && y >= (y1-t) && y <= (y2+t);
//    _dragY1 = qAbs(y1-y) < t && x >= (x1-t) && x <= (x2+t);
//    _dragY2 = qAbs(y2-y) < t && x >= (x1-t) && x <= (x2+t);

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
        resetDragCusrsor();
}

void ApertureRect::mousePress(QMouseEvent *e)
{
    if (!visible() || !_editing) return;
    if (e->button() != Qt::LeftButton) return;
    _dragging = true;
    _dragX = e->pos().x();
    _dragY = e->pos().y();
    showCoordTooltip(e->globalPosition().toPoint());
}

void ApertureRect::mouseRelease(QMouseEvent *e)
{
    if (!visible() || !_editing) return;
    QToolTip::hideText();
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

void ApertureRect::showCoordTooltip(const QPoint &p)
{
    QStringList hint;
    if (_drag0 || _dragX1) hint << QStringLiteral("X1: %1").arg(int(_x1));
    if (_drag0 || _dragX2) hint << QStringLiteral("X2: %1").arg(int(_x2));
    if (_drag0 || _dragY1) hint << QStringLiteral("Y1: %1").arg(int(_y1));
    if (_drag0 || _dragY2) hint << QStringLiteral("Y2: %1").arg(int(_y2));
    if (_dragX1 || _dragX2) hint << QStringLiteral("W: %1").arg(int(_x2) - int(_x1));
    if (_dragY1 || _dragY2) hint << QStringLiteral("H: %1").arg(int(_y2) - int(_y1));
    if (!hint.isEmpty()) QToolTip::showText(p, hint.join('\n'));
}
