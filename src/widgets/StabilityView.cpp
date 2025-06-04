#include "StabilityView.h"

#include "helpers/OriLayouts.h"
#include "helpers/OriWidgets.h"
#include "plot/Heatmap.h"
#include "widgets/OriPopupMessage.h"

#include "qcustomplot/src/core.h"
#include "qcustomplot/src/axis/axistickerdatetime.h"
#include "qcustomplot/src/layoutelements/layoutelement-axisrect.h"
#include "qcustomplot/src/layoutelements/layoutelement-legend.h"
#include "qcustomplot/src/layoutelements/layoutelement-textelement.h"
#include "qcustomplot/src/plottables/plottable-graph.h"

#define A_ Ori::Gui::action

#define UPDATE_INTERVAL_MS 1000
#define CLEAN_INTERVAL_MS 60000
#define DATA_BUF_SIZE 10
#define HEATMAP_SPLIT_COUNT 5

StabilityView::StabilityView(QWidget *parent) : QWidget{parent}
{
    _dataBuf.resize(10);
    _timelineMinV = qQNaN();
    _timelineMaxV = qQNaN();

    _plotTime = new QCustomPlot;
    _plotHeat = new QCustomPlot;

    _plotTime->setContextMenuPolicy(Qt::ActionsContextMenu);
    _plotHeat->setContextMenuPolicy(Qt::ActionsContextMenu);

    _plotTime->legend->setVisible(true);
    _plotTime->yAxis->setPadding(8);
    
    // By default, the legend is in the inset layout of the main axis rect.
    // So this is how we access it to change legend placement:
    _plotTime->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignLeft|Qt::AlignTop);
    
    QSharedPointer<QCPAxisTickerDateTime> timeTicker(new QCPAxisTickerDateTime);
    timeTicker->setDateTimeSpec(Qt::LocalTime);
    timeTicker->setDateTimeFormat("hh:mm:ss\nMMM dd");
    _plotTime->xAxis->setTicker(timeTicker);
    
    _timelineX = _plotTime->addGraph();
    _timelineY = _plotTime->addGraph();
    _timelineX->setName("X");
    _timelineY->setName("Y");
    
    _heatmapData = new HeatmapData(HEATMAP_SPLIT_COUNT, HEATMAP_SPLIT_COUNT);
    _heatmap = new Heatmap(_plotHeat->xAxis, _plotHeat->yAxis);
    _heatmap->setData(_heatmapData);
    _heatmap->setGradient(QCPColorGradient(QCPColorGradient::gpThermal));
    _heatmap->setInterpolate(false);
    
    _timelineDurationText = new QCPTextElement(_plotTime);
    
    _plotTime->axisRect()->insetLayout()->addElement(_timelineDurationText, Qt::AlignRight|Qt::AlignTop);

    auto f = _plotTime->yAxis->labelFont();
    f.setPointSize(f.pointSize()+1);
    _plotHeat->xAxis->setLabelFont(f);

    // Vertically oriented font looks a bit ugly
    // especially on narrow letters "i", "t", like in "pos_i_t_i_on"
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1);
    _plotTime->yAxis->setLabelFont(f);
    _plotHeat->yAxis->setLabelFont(f);
    
    setThemeColors(PlotHelpers::SYSTEM, false);
    
    _plotTime->addAction(A_(tr("Reset"), this, &StabilityView::cleanResult, ":/toolbar/update"));
    _plotTime->addAction(Ori::Gui::separatorAction(this));
    _plotTime->addAction(A_(tr("Copy X Points"), this, [this]{ copyGraph(_timelineX); }));
    _plotTime->addAction(A_(tr("Copy Y Points"), this, [this]{ copyGraph(_timelineY); }));
    
    _plotTime->addAction(A_(tr("Copy as Image"), this, [this]{
        PlotHelpers::copyImage(_plotTime, [this](PlotHelpers::Theme theme){ setThemeColors(theme, false); });
    }, ":/toolbar/copy_img"));
    
    _plotHeat->addAction(A_(tr("Copy as Image"), this, [this]{
        PlotHelpers::copyImage(_plotHeat, [this](PlotHelpers::Theme theme){ setThemeColors(theme, false); });
    }, ":/toolbar/copy_img"));
    
    Ori::Layouts::LayoutH({_plotTime, _plotHeat}).setMargin(0).useFor(this);
}

void StabilityView::setThemeColors(PlotHelpers::Theme theme, bool replot)
{
    PlotHelpers::setThemeColors(_plotTime, theme);
    PlotHelpers::setThemeColors(_plotHeat, theme);
    _timelineDurationText->setTextColor(PlotHelpers::themeAxisLabelColor(theme));
    _timelineX->setPen(QPen(QColor(0, 0, 225, 125)));
    _timelineY->setPen(QPen(QColor(255, 0, 0, 125)));
    if (replot) {
        _plotTime->replot();
        _plotHeat->replot();
    }
}

void StabilityView::restoreState(QSettings *s)
{
}

void StabilityView::setConfig(const PixelScale& scale, const Stability &stabil)
{
    if (_scale != scale) {
        _scale = scale;
        resetScale(false, true);
    }
    
    if (stabil.displayMins * 60 < _timelineDisplayS) {
        // force clean on the next update
        _cleanTimeMs = 0;
    }
    _timelineDisplayS = stabil.displayMins * 60;
    
    if (!stabil.axisText.isEmpty()) {
        QString text = stabil.axisText;
        QString unit;
        if (_scale.on && !_scale.unit.isEmpty())
            unit = QString(" [%1]").arg(_scale.unit);
        _plotTime->yAxis->setLabel(text % unit);
        _plotHeat->xAxis->setLabel(text % " X" % unit);
        _plotHeat->yAxis->setLabel(text % " Y" % unit);
    } else {
        _plotTime->yAxis->setLabel(QString());
        _plotHeat->xAxis->setLabel(QString());
        _plotHeat->yAxis->setLabel(QString());
    }
}

void StabilityView::setResult(qint64 time, const QList<CgnBeamResult>& val)
{
    if (!_turnedOn)
        return;

    if (val.isEmpty())
        return;

    // Only first ROI
    const auto &r = val.first();
    if (r.nan)
        return;
        
    if (_dataBufCursor >= DATA_BUF_SIZE)
        return;
        
    _dataBuf[_dataBufCursor] = DataPoint {
        .ms = time,
        .x = r.xc,
        .y = r.yc,
    };
    _dataBufCursor++;
    _frameTimeMs = time;
    if (_startTimeMs < 0)
        _startTimeMs = _frameTimeMs;
}

void StabilityView::showResult()
{
    if (!_turnedOn)
        return;

    if (!(_showTimeMs < 0 || _frameTimeMs - _showTimeMs >= UPDATE_INTERVAL_MS))
        return;
        
    int newPoints = _dataBufCursor;
    bool valueRangeChanged = false;
    for (int i = 0; i < newPoints; i++) {
        const auto &p = _dataBuf.at(i);
        const double sec = p.ms / 1000.0;
        
        if (_offsetVx < 0) _offsetVx = p.x;
        if (_offsetVy < 0) _offsetVy = p.y;

        const double timelineX = _scale.pixelToUnit(p.x - _offsetVx);
        const double timelineY = _scale.pixelToUnit(p.y - _offsetVy);

        _timelineX->addData(sec, timelineX);
        _timelineY->addData(sec, timelineY);

        const double timelineMaxV = qMax(timelineX, timelineY);
        if (_timelineMaxV < timelineMaxV || qIsNaN(_timelineMaxV))
            _timelineMaxV = timelineMaxV, valueRangeChanged = true;
        
        const double timelineMinV = qMin(timelineX, timelineY);
        if (_timelineMinV > timelineMinV || qIsNaN(_timelineMinV))
            _timelineMinV = timelineMinV, valueRangeChanged = true;

        _timelineMaxS = sec;
        if (_timelineMinS < 0)
            _timelineMinS = sec;
    }
    
    if (valueRangeChanged) {
        const double m = (_timelineMaxV - _timelineMinV) * 0.1;
        _plotTime->yAxis->setRange(_timelineMinV - m, _timelineMaxV + m);
        recalcHeatmap();
    } else {
        updateHeatmap(newPoints);
    }

    // Don't clean invisible points
    // They will be needed to recalculate the heatmap when the ranhe changed
    //
    // if (_cleanTimeMs < 0) {
    //     _cleanTimeMs = _frameTimeMs;
    // } else if (_frameTimeMs - _cleanTimeMs >= CLEAN_INTERVAL_MS) {
    //     if (_timelineMaxS - _timelineMinS > _timelineDisplayS) {
    //         _timelineMinS = _timelineMaxS - _timelineDisplayS;
    //         _timelineX->data()->removeBefore(_timelineMinS);
    //         _timelineY->data()->removeBefore(_timelineMinS);
    //         _cleanTimeMs = _frameTimeMs;
    //     }
    // } 
    
    _plotTime->xAxis->setRange(timelineDisplayMinS(), _timelineMaxS);

    _timelineDurationText->setText(formatSecs((_frameTimeMs - _startTimeMs) / 1000));

    _showTimeMs = _frameTimeMs;
    _dataBufCursor = 0;

    // In non-active mode the panel is still visbile but temporary overlapped by another tab
    // So it still should process data. When we switch to its tab, plots must be shown properly
    if (_active) {
        _plotTime->replot();
        _plotHeat->replot();
    }
}

double StabilityView::timelineDisplayMinS() const
{
    return qMax(_timelineMinS, _timelineMaxS - _timelineDisplayS);
}

void StabilityView::cleanResult()
{
    _timelineX->data()->clear();
    _timelineY->data()->clear();
    _frameTimeMs = -1;
    _showTimeMs = -1;
    _cleanTimeMs = -1;
    _startTimeMs = -1;
    _offsetVx = -1;
    _offsetVy = -1;
    _dataBufCursor = 0;
    _timelineDurationText->setText(QString());
    resetScale(true, true);
}

void StabilityView::resetScale(bool time, bool value)
{
    if (time) {
        _timelineMinS = -1;
        _timelineMaxS = -1;
    }
    if (value) {
        _timelineMinV = qQNaN();
        _timelineMaxV = qQNaN();
    }
}

void StabilityView::copyGraph(QCPGraph *graph)
{
    QString res;
    QTextStream s(&res);
    double startS = timelineDisplayMinS();
    foreach (const auto& p, graph->data()->rawData()) {
        if (p.key < startS)
            continue;
        QString timeStr = QDateTime::fromMSecsSinceEpoch(p.key * 1000.0).toString(Qt::ISODateWithMs);
        s << timeStr << ',' << QString::number(p.value) << '\n';
    }
    qApp->clipboard()->setText(res);
    Ori::Gui::PopupMessage::affirm(qApp->tr("Data points been copied to Clipboard"), Qt::AlignRight|Qt::AlignBottom);
}

void StabilityView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    
    _plotHeat->setFixedWidth(_plotHeat->height());
}

void StabilityView::activate(bool on)
{
    _active = on;

    if (_active) {
        _plotTime->replot();
        _plotHeat->replot();
    }
}

void StabilityView::turnOn(bool on)
{
    if (!on) {
        cleanResult();
        _plotTime->replot();
        _plotHeat->replot();
    }
    _turnedOn = on;
}

void StabilityView::recalcHeatmap()
{
    const auto xs = _timelineX->data();
    const auto ys = _timelineY->data();
    if (xs->size() != ys->size()) {
        qCritical() << "Unequal point count in timeline graphs" << xs->size() << ys->size();
        return;
    }
    int maxCellPoints = 0;
    QCPRange r(qMin(_timelineMinV, -0.1), qMax(_timelineMaxV, 0.1));
    _heatmapData->fill(0);
    _heatmapData->setRange(r, r);
    const int pointCount = xs->size();
    for (int i = 0; i < pointCount; i++) {
        int cellIndexX, cellIndexY;
        _heatmapData->coordToCell(xs->at(i)->value, ys->at(i)->value, &cellIndexX, &cellIndexY);
        int cellPoints = int(_heatmapData->cell(cellIndexX, cellIndexY)) + 1;
        if (cellPoints > maxCellPoints) maxCellPoints = cellPoints;
        _heatmapData->setCell(cellIndexX, cellIndexY, cellPoints);
    }
    _heatmap->setDataRange(QCPRange(0, maxCellPoints));
    _plotHeat->xAxis->setRange(r);
    _plotHeat->yAxis->setRange(r);
}

void StabilityView::updateHeatmap(int lastPoints)
{
    const auto xs = _timelineX->data();
    const auto ys = _timelineY->data();
    if (xs->size() != ys->size()) {
        qCritical() << "Unequal point count in timeline graphs" << xs->size() << ys->size();
        return;
    }
    const int pointCount = xs->size();
    const int startIndex = pointCount - lastPoints;
    int maxCellPoints = _heatmap->dataRange().upper;
    bool dataRangeChanged = false;
    for (int i = startIndex; i < pointCount; i++) {
        int cellIndexX, cellIndexY;
        _heatmapData->coordToCell(xs->at(i)->value, ys->at(i)->value, &cellIndexX, &cellIndexY);
        int cellPoints = int(_heatmapData->cell(cellIndexX, cellIndexY)) + 1;
        if (cellPoints > maxCellPoints) maxCellPoints = cellPoints, dataRangeChanged = true;
        _heatmapData->setCell(cellIndexX, cellIndexY, cellPoints);
    }
    if (dataRangeChanged) {
        _heatmap->setDataRange(QCPRange(0, maxCellPoints));
    }
}