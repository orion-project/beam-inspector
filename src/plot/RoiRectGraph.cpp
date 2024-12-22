#include "RoiRectGraph.h"

#include "app/AppSettings.h"

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

RoiRectGraph::RoiRectGraph(QCustomPlot *parentPlot) : BeamPlotItem(parentPlot)
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

void RoiRectGraph::updateVisibility()
{
    setVisible(_editing || _isVisible);
}

void RoiRectGraph::updateCoords()
{
    _x1 = _scale.pixelToUnit(_roi.left * _maxPixelX);
    _y1 = _scale.pixelToUnit(_roi.top * _maxPixelY);
    _x2 = _scale.pixelToUnit(_roi.right * _maxPixelX);
    _y2 = _scale.pixelToUnit(_roi.bottom * _maxPixelY);
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
        _roi.left = _scale.unitToPixel(_x1) / _maxPixelX;
        _roi.top = _scale.unitToPixel(_y1) / _maxPixelY;
        _roi.right = _scale.unitToPixel(_x2) / _maxPixelX;
        _roi.bottom = _scale.unitToPixel(_y2) / _maxPixelY;
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
            _x2 = qMin(_x2, _maxUnitX);
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
            _y2 = qMin(_y2, _maxUnitY);
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
    _seX1 = new RoiBoundEdit(_x1, _maxUnitX, +1);
    _seY1 = new RoiBoundEdit(_y1, _maxUnitY, -1);
    _seX2 = new RoiBoundEdit(_x2, _maxUnitX, +1);
    _seY2 = new RoiBoundEdit(_y2, _maxUnitY, -1);
    _seW = new RoiBoundEdit(roiW(), _maxUnitX, 1);
    _seH = new RoiBoundEdit(roiH(), _maxUnitY, 1);
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

//------------------------------------------------------------------------------
//                               RoiRectsGraph
//------------------------------------------------------------------------------

RoiRectsGraph::RoiRectsGraph(QCustomPlot *parentPlot) : BeamPlotItem(parentPlot)
{
    _pen = QPen(Qt::yellow, 0, Qt::SolidLine);
    _penBad = QPen(Qt::red, 0, Qt::SolidLine);
    _penGood = QPen(Qt::green, 0, Qt::SolidLine);
    _brush = QBrush(Qt::yellow);
    _brushBad = QBrush(Qt::red);
    _brushGood = QBrush(Qt::green);
}

void RoiRectsGraph::setRois(const QList<RoiRect> &rois, const QList<GoodnessLimits> &goodnessLimits)
{
    _relRois = rois;

    _relGoodnessLimits.clear();
    for (int i = 0; i < _relRois.size(); i++) {
        if (i < goodnessLimits.size())
            _relGoodnessLimits << goodnessLimits.at(i);
        else _relGoodnessLimits << GoodnessLimits();
    }

    updateCoords();

    _showGoodnessTextOnPlot = AppSettings::instance().showGoodnessTextOnPlot;
    _showGoodnessRelative = AppSettings::instance().showGoodnessRelative;
}

void RoiRectsGraph::updateCoords()
{
    _unitRois.clear();
    _goodness.clear();
    _unitGoodnessLimits.clear();
    for (int i = 0; i < _relRois.size(); i++) {
        const auto &relRoi = _relRois.at(i);
        RoiRect unitRoi;
        unitRoi.left = _scale.pixelToUnit(_maxPixelX * relRoi.left);
        unitRoi.top = _scale.pixelToUnit(_maxPixelY * relRoi.top);
        unitRoi.right = _scale.pixelToUnit(_maxPixelX * relRoi.right);
        unitRoi.bottom = _scale.pixelToUnit(_maxPixelY * relRoi.bottom);
        _unitRois << unitRoi;

        const auto &relLim = _relGoodnessLimits.at(i);
        GoodnessLimits unitLim;
        unitLim.deltaMin = _scale.pixelToUnit(_maxPixelX * relLim.deltaMin);
        unitLim.deltaMax = _scale.pixelToUnit(_maxPixelX * relLim.deltaMax);
        _unitGoodnessLimits << unitLim;

        _goodness << GoodnessData();
    }
}

void RoiRectsGraph::draw(QCPPainter *painter)
{
    for (int i = 0; i < _unitRois.size(); i++) {
        const auto& r = _unitRois.at(i);
        const auto& g = _goodness.at(i);

        if (!g.good)
            painter->setPen(_pen);
        else if (*g.good)
            painter->setPen(_penGood);
        else
            painter->setPen(_penBad);

        const double x1 = qRound(parentPlot()->xAxis->coordToPixel(r.left));
        const double y1 = qRound(parentPlot()->yAxis->coordToPixel(r.top));
        const double x2 = qRound(parentPlot()->xAxis->coordToPixel(r.right));
        const double y2 = qRound(parentPlot()->yAxis->coordToPixel(r.bottom));
        const double w = x2 - x1;
        const double h = y2 - y1;
        painter->drawRect(x1, y1, w, h);

        const double gh = qRound(h * g.goodness);
        //const double gw = qRound(w * 0.1);
        const double gw = 8;
        painter->fillRect(x2, y2-gh, gw, gh+1, !g.good ? _brush : *g.good ? _brushGood : _brushBad);

        if (_showGoodnessTextOnPlot) {
            const auto &relLimits = _relGoodnessLimits.at(i);
            const auto &unitLimits = _unitGoodnessLimits.at(i);
            const auto &relRoi = _relRois.at(i);
            painter->drawText(QRect(x2 + gw + 2, y1, 1, h), Qt::TextDontClip,
                QString(
                    "goodness: %1\n"
                    "delta: %2\n"
                    "delta_min: %3\n"
                    "delta_max: %4\n"
                    "roi: %5×%6"
                )
                .arg(g.goodness, 0, 'g', 4)
                .arg(_showGoodnessRelative ? g.distance/_maxPixelX : g.distance, 0, 'g', 4)
                .arg(_showGoodnessRelative ? relLimits.deltaMin : unitLimits.deltaMin, 0, 'g', 4)
                .arg(_showGoodnessRelative ? relLimits.deltaMax : unitLimits.deltaMax, 0, 'g', 4)
                .arg(_showGoodnessRelative ? (relRoi.right - relRoi.left) : (r.right - r.left), 0, 'g', 4)
                .arg(_showGoodnessRelative ? (relRoi.bottom - relRoi.top) : (r.bottom - r.top), 0, 'g', 4)
            );
        }
    }
}

void RoiRectsGraph::drawGoodness(int index, double beamXc, double beamYc)
{
    // This is visible on ROI_MULTI mode, and all list sizes should must match
    if (!visible()) return;

    const auto &roi = _unitRois.at(index);
    const double roiXc = (roi.left + roi.right) / 2.0;
    const double roiYc = (roi.top + roi.bottom) / 2.0;
    const double dist = qHypot(beamXc - roiXc, beamYc - roiYc);
    const auto &limits = _unitGoodnessLimits.at(index);
    GoodnessData g;
    g.distance = dist;
    if (dist < limits.deltaMin) {
        g.good = true;
        g.goodness = 1;
    } else if (dist >= limits.deltaMax) {
        g.good = false;
        g.goodness = 0;
    } else {
        g.goodness = 1.0 - (dist - limits.deltaMin) / (limits.deltaMax - limits.deltaMin);
    }
    _goodness[index] = g;
}
