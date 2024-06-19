#include "RoiRectGraph.h"

#include "qcp/src/core.h"
#include "qcp/src/painter.h"
#include "qcp/src/layoutelements/layoutelement-axisrect.h"

#include <QBoxLayout>
#include <QFormLayout>
#include <QGraphicsDropShadowEffect>
#include <QSpinBox>
#include <QToolButton>

//------------------------------------------------------------------------------
//                               RoiBoundEdit
//------------------------------------------------------------------------------

class RoiBoundEdit : public QSpinBox
{
public:
    RoiBoundEdit(int value, int max, int sign) : QSpinBox(), _sign(sign) {
        setMinimum(0);
        setMaximum(max);
        setValue(value);
    }
protected:
    void keyPressEvent(QKeyEvent *e) override {
        if (e->modifiers().testFlag(Qt::ShiftModifier)) {
            QSpinBox::keyPressEvent(e);
            return;
        }
        switch (e->key()) {
        case Qt::Key_Up:
        case Qt::Key_Right:
            stepBy(_sign * (e->modifiers().testFlag(Qt::ControlModifier) ? 100 : 1));
            break;
        case Qt::Key_Down:
        case Qt::Key_Left:
            stepBy(_sign * (e->modifiers().testFlag(Qt::ControlModifier) ? -100 : -1));
            break;
        default:
            QSpinBox::keyPressEvent(e);
        }
    }
private:
    int _sign;
};

//------------------------------------------------------------------------------
//                               RoiRectGraph
//------------------------------------------------------------------------------

RoiRectGraph::RoiRectGraph(QCustomPlot *parentPlot) : QCPAbstractItem(parentPlot)
{
    _pen = QPen(Qt::yellow, 0, Qt::DashLine);
    _editPen = QPen(Qt::yellow, 3, Qt::SolidLine);

    connect(parentPlot, &QCustomPlot::mouseMove, this, &RoiRectGraph::mouseMove);
    connect(parentPlot, &QCustomPlot::mousePress, this, &RoiRectGraph::mousePress);
    connect(parentPlot, &QCustomPlot::mouseRelease, this, &RoiRectGraph::mouseRelease);
    connect(parentPlot, &QCustomPlot::mouseDoubleClick, this, &RoiRectGraph::mouseDoubleClick);
}

void RoiRectGraph::setRoi(const RoiRect &roi)
{
    _roi = roi;
    updateCoords();
    updateVisibility();
}

void RoiRectGraph::setIsVisible(bool on)
{
    _isVisible = on;
    updateVisibility();
}

void RoiRectGraph::setImageSize(int sensorW, int sensorH, const PixelScale &scale)
{
    _scale = scale;
    _maxX = scale.pixelToUnit(sensorW);
    _maxY = scale.pixelToUnit(sensorH);
    updateCoords();
}

void RoiRectGraph::updateVisibility()
{
    setVisible(_editing || (_roi.on && _isVisible));
}

void RoiRectGraph::updateCoords()
{
    _x1 = _scale.pixelToUnit(_roi.x1);
    _y1 = _scale.pixelToUnit(_roi.y1);
    _x2 = _scale.pixelToUnit(_roi.x2);
    _y2 = _scale.pixelToUnit(_roi.y2);
}

void RoiRectGraph::startEdit()
{
    if (_editing)
        return;
    _editing = true;
    makeEditor();
    updateVisibility();
    parentPlot()->replot();
}

void RoiRectGraph::stopEdit(bool apply)
{
    if (!_editing)
        return;
    _editing = false;
    if (apply) {
        _roi.x1 = _scale.unitToPixel(_x1);
        _roi.y1 = _scale.unitToPixel(_y1);
        _roi.x2 = _scale.unitToPixel(_x2);
        _roi.y2 = _scale.unitToPixel(_y2);
        _roi.on = true;
        if (onEdited)
            onEdited();
    } else {
        updateCoords();
    }
    updateVisibility();
    _editor->deleteLater();
    _editor = nullptr;
    QToolTip::hideText();
    resetDragCusrsor();
    parentPlot()->replot();
}

static const int dragThreshold = 10;

void RoiRectGraph::draw(QCPPainter *painter)
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

void RoiRectGraph::mouseMove(QMouseEvent *e)
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
            _x1 = parentPlot()->xAxis->pixelToCoord(x1+dx);
            _x1 = qMax(_x1, 0.0);
            _seX1->setValue(_x1);
            _seW->setValue(roiW());
        }
        if (_drag0 || _dragX2) {
            _x2 = parentPlot()->xAxis->pixelToCoord(x2+dx);
            _x2 = qMin(_x2, _maxX);
            _seX2->setValue(_x2);
            _seW->setValue(roiW());
        }
        if (_drag0 || _dragY1) {
            _y1 = parentPlot()->yAxis->pixelToCoord(y1+dy);
            _y1 = qMax(_y1, 0.0);
            _seY1->setValue(_y1);
            _seH->setValue(roiH());
        }
        if (_drag0 || _dragY2) {
            _y2 = parentPlot()->yAxis->pixelToCoord(y2+dy);
            _y2 = qMin(_y2, _maxY);
            _seY2->setValue(_y2);
            _seH->setValue(roiH());
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

void RoiRectGraph::mousePress(QMouseEvent *e)
{
    if (!visible() || !_editing) return;
    if (e->button() != Qt::LeftButton) return;
    _dragging = true;
    _dragX = e->pos().x();
    _dragY = e->pos().y();
    showCoordTooltip(e->globalPosition().toPoint());
}

void RoiRectGraph::mouseRelease(QMouseEvent *e)
{
    if (!visible() || !_editing) return;
    QToolTip::hideText();
    _dragging = false;
}

void RoiRectGraph::mouseDoubleClick(QMouseEvent*)
{
    if (!visible() || !_editing) return;
    stopEdit(true);
}

void RoiRectGraph::showDragCursor(Qt::CursorShape c)
{
    if (_dragCursor == c)
        return;
    _dragCursor = c;
    QApplication::restoreOverrideCursor();
    if (c != Qt::ArrowCursor)
        QApplication::setOverrideCursor(c);
}

void RoiRectGraph::showCoordTooltip(const QPoint &p)
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


void RoiRectGraph::makeEditor()
{
    _seX1 = new RoiBoundEdit(_x1, _maxX, +1);
    _seY1 = new RoiBoundEdit(_y1, _maxY, -1);
    _seX2 = new RoiBoundEdit(_x2, _maxX, +1);
    _seY2 = new RoiBoundEdit(_y2, _maxY, -1);
    _seW = new RoiBoundEdit(roiW(), _maxX, 1);
    _seH = new RoiBoundEdit(roiH(), _maxY, 1);
    _seW->setReadOnly(true);
    _seH->setReadOnly(true);

    connect(_seX1, &QSpinBox::valueChanged, this, [this](int v){
        if ((int)_x1 == v) return;
        _x1 = v;
        _seW->setValue(roiW());
        parentPlot()->replot();
    });
    connect(_seX2, &QSpinBox::valueChanged, this, [this](int v){
        if ((int)_x2 == v) return;
        _x2 = v;
        _seW->setValue(roiW());
        parentPlot()->replot();
    });
    connect(_seY1, &QSpinBox::valueChanged, this, [this](int v){
        if ((int)_y1 == v) return;
        _y1 = v;
        _seH->setValue(roiH());
        parentPlot()->replot();
    });
    connect(_seY2, &QSpinBox::valueChanged, this, [this](int v){
        if ((int)_y2 == v) return;
        _y2 = v;
        _seH->setValue(roiH());
        parentPlot()->replot();
    });

    _editor = new QFrame;
    _editor->setFrameShape(QFrame::NoFrame);
    _editor->setFrameShadow(QFrame::Plain);
    auto shadow = new QGraphicsDropShadowEffect;
    if (qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark)
        shadow->setColor(QColor(255, 255, 255, 180));
    else
        shadow->setColor(QColor(0, 0, 0, 180));
    shadow->setBlurRadius(20);
    shadow->setOffset(0);
    _editor->setGraphicsEffect(shadow);

    auto butApply = new QToolButton;
    auto butCancel = new QToolButton;
    butApply->setIcon(QIcon(":/toolbar/check"));
    butCancel->setIcon(QIcon(":/toolbar/delete"));
    connect(butApply, &QToolButton::pressed, this, [this]{ stopEdit(true); });
    connect(butCancel, &QToolButton::pressed, this, [this]{ stopEdit(false); });
    auto buttons = new QHBoxLayout;
    buttons->addWidget(butApply);
    buttons->addWidget(butCancel);

    auto l = new QFormLayout(_editor);
    l->addRow("X1", _seX1);
    l->addRow("Y1", _seY1);
    l->addRow("X2", _seX2);
    l->addRow("Y2", _seY2);
    l->addRow("W", _seW);
    l->addRow("H", _seH);
    l->addRow("", buttons);

    _editor->setParent(parentPlot());
    _editor->setAutoFillBackground(true);
    _editor->show();
    _editor->adjustSize();
    adjustEditorPosition();
}

void RoiRectGraph::adjustEditorPosition()
{
    if (_editor)
        _editor->move(parentPlot()->width() - _editor->width() - 15, 15);
}
