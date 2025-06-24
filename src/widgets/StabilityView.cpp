#include "StabilityView.h"

#include "app/AppSettings.h"
#include "app/HelpSystem.h"
#include "helpers/OriLayouts.h"
#include "helpers/OriWidgets.h"
#include "plot/BeamGraph.h"
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
#define LOG_ID "StabilityView:"
//#define LOG_EX

StabilityView::StabilityView(QWidget *parent) : QWidget{parent}
{
    _dataBuf.resize(10);
    resetScale(true, true);

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
    _plotTime->yAxis->setRangeReversed(true);
    
    _timelineX = _plotTime->addGraph();
    _timelineY = _plotTime->addGraph();
    _timelineX->setName("X");
    _timelineY->setName("Y");
    
    PrecalculatedGradient heatGradient(AppSettings::colorMapPath("viridis_r"));
    heatGradient.setNanHandling(QCPColorGradient::nhNanColor);
    heatGradient.setNanColor(Qt::white);
    
    _heatmapData = new HeatmapData;
    _heatmap = new Heatmap(_plotHeat->xAxis, _plotHeat->yAxis);
    _heatmap->setData(_heatmapData);
    _heatmap->setInterpolate(false);
    _heatmap->setGradient(heatGradient);

    _colorScale = new BeamColorScale(_plotHeat);
    _colorScale->setBarWidth(6);
    _colorScale->axis()->setPadding(10);
    _colorScale->axis()->setTicks(false);
    _colorScale->setGradient(heatGradient);
    
    _plotHeat->plotLayout()->addElement(0, 1, _colorScale);
    _plotHeat->yAxis->setRangeReversed(true);
    _plotHeat->axisRect()->setupFullAxesBox(true);
    _plotHeat->yAxis2->setTicks(false);
    _plotHeat->xAxis2->setTicks(false);
    
    _timelineDurationText = new QCPTextElement(_plotTime);
    
    _plotTime->axisRect()->insetLayout()->addElement(_timelineDurationText, Qt::AlignRight|Qt::AlignTop);

    // auto f = _plotTime->yAxis->labelFont();
    // f.setPointSize(f.pointSize()+1);
    // _plotHeat->xAxis->setLabelFont(f);

    // // Vertically oriented font looks a bit ugly
    // // especially on narrow letters "i", "t", like in "pos_i_t_i_on"
    // f.setLetterSpacing(QFont::AbsoluteSpacing, 1);
    // _plotTime->yAxis->setLabelFont(f);
    // _plotHeat->yAxis->setLabelFont(f);
    
    setThemeColors(PlotHelpers::SYSTEM, false);
    
    auto actnReset = A_(tr("Reset"), this, &StabilityView::cleanResult, ":/toolbar/update");
    auto actnHelp = A_(tr("Help"), this, []{ HelpSystem::topic("stability_view"); }, ":/ori_images/help");
    auto actnSep1 = Ori::Gui::separatorAction(this);
    auto actnSep2 = Ori::Gui::separatorAction(this);
    
    _plotTime->addAction(actnReset);
    _plotTime->addAction(actnSep1);
    _plotTime->addAction(A_(tr("Copy X Points"), this, [this]{ copyGraph(_timelineX); }));
    _plotTime->addAction(A_(tr("Copy Y Points"), this, [this]{ copyGraph(_timelineY); }));
    _plotTime->addAction(A_(tr("Copy X and Y Points"), this, [this]{ copyGraph(); }, ":/toolbar/copy"));
    _plotTime->addAction(A_(tr("Copy as Image"), this, [this]{
        PlotHelpers::copyImage(_plotTime, [this](PlotHelpers::Theme theme){ setThemeColors(theme, false); });
    }, ":/toolbar/copy_img"));
    _plotTime->addAction(actnSep2);
    _plotTime->addAction(actnHelp);
    
    _plotHeat->addAction(actnReset);
    _plotHeat->addAction(actnSep1);
    _plotHeat->addAction(A_(tr("Copy as Image"), this, [this]{
        PlotHelpers::copyImage(_plotHeat, [this](PlotHelpers::Theme theme){ setThemeColors(theme, false); });
    }, ":/toolbar/copy_img"));
    _plotHeat->addAction(actnSep2);
    _plotHeat->addAction(actnHelp);
    
    Ori::Layouts::LayoutH({_plotTime, _plotHeat}).setMargin(0).useFor(this);
}

void StabilityView::setThemeColors(PlotHelpers::Theme theme, bool replot)
{
    PlotHelpers::setThemeColors(_plotTime, theme);
    PlotHelpers::setThemeColors(_plotHeat, theme);
    _timelineDurationText->setTextColor(PlotHelpers::themeAxisLabelColor(theme));
    _colorScale->setFrameColor(PlotHelpers::themeAxisColor(theme));
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

#ifdef LOG_EX
struct MethodLog
{
    MethodLog(const char *name): name(name)
    {
        qDebug() << LOG_ID << name << "beg";
    }
    
    ~MethodLog()
    {
        qDebug() << LOG_ID  << name << "end";
    }
    
    const char *name;
};
#endif

void StabilityView::setConfig(const PixelScale& scale, const Stability &stabil)
{
#ifdef LOG_EX
    MethodLog _("setConig");
#endif
    qDebug().noquote().nospace() << LOG_ID << " setConfig: "
        << "scale=" << scale.str() << ", stabil=" << stabil.str();

    if (_heatmapCellCount != stabil.heatmapCells) {
        _heatmapCellCount = stabil.heatmapCells;
        _heatmapData->clear();
    }
    
    if (_scale != scale) {
        rescaleTimeline(scale);
        recalcHeatmap();
        _scale = scale;
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

#ifdef LOG_EX
    MethodLog _("setResult");
#endif
    
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
        
#ifdef LOG_EX
    MethodLog _("showResult");
    qDebug() << LOG_ID << _dataBufCursor;
#endif

    int newPoints = _dataBufCursor;
    if (newPoints < 1) return;
    
    bool valueRangeChanged = false;
    for (int i = 0; i < newPoints; i++) {
        const auto &p = _dataBuf.at(i);
        const double sec = p.ms / 1000.0;
        
        if (_offsetX < 0) _offsetX = p.x;
        if (_offsetY < 0) _offsetY = p.y;

        const double X = _scale.pixelToUnit(p.x - _offsetX);
        const double Y = _scale.pixelToUnit(p.y - _offsetY);

        _timelineX->addData(sec, X);
        _timelineY->addData(sec, Y);

        if (adjustValueRange(X, Y))
            valueRangeChanged = true;

        _timelineMaxS = sec;
        if (_timelineMinS < 0)
            _timelineMinS = sec;
    }
    
    // _heatmapData gets cleaned when _heatmapCellCount is changed
    // all ranges could stay the same then, but the full recalc is required
    if (valueRangeChanged || _heatmapData->isEmpty()) {
        adjustTimelineVertAxis();
        recalcHeatmap();
    } else {
        updateHeatmap(newPoints);
    }

    // Don't clean invisible points
    // They will be needed to recalculate the heatmap when the range is adjusted
    // or the number of cells is changed by user
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
#ifdef LOG_EX
    MethodLog _("cleanResult");
#endif

    _timelineX->data()->clear();
    _timelineY->data()->clear();
    _frameTimeMs = -1;
    _showTimeMs = -1;
    _cleanTimeMs = -1;
    _startTimeMs = -1;
    _offsetX = -1;
    _offsetY = -1;
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
        _valueMin = qQNaN();
        _valueMax = qQNaN();
        _valueMinX = qQNaN();
        _valueMaxX = qQNaN();
        _valueMinY = qQNaN();
        _valueMaxY = qQNaN();
    }
}

void StabilityView::copyGraph(QCPGraph *graph)
{
    QString res;
    QTextStream s(&res);
    if (graph) {
        s << "Index,Timestamp," << (graph == _timelineX ? "X" : "Y") << '\n';
        const auto d = graph->data();
        for (int i = 0; i < d->size(); i++) {
            s << i
              << ',' << QDateTime::fromMSecsSinceEpoch(d->at(i)->key * 1000.0).toString(Qt::ISODateWithMs)
              << ',' << QString::number(d->at(i)->value)
              << '\n';
        }
    } else {
        const auto xs = _timelineX->data();
        const auto ys = _timelineY->data();
        const auto pointCount = qMin(xs->size(), ys->size()); // Should not differ
        s << "Index,Timestamp,X,Y\n";
        for (int i = 0; i < pointCount; i++) {
            s << i
              << ',' << QDateTime::fromMSecsSinceEpoch(xs->at(i)->key * 1000.0).toString(Qt::ISODateWithMs)
              << ',' << QString::number(xs->at(i)->value)
              << ',' << QString::number(ys->at(i)->value)
              << '\n';
        }
    }
    qApp->clipboard()->setText(res);
    Ori::Gui::PopupMessage::affirm(qApp->tr("Data points been copied to Clipboard"), Qt::AlignRight|Qt::AlignBottom);
}

void StabilityView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    
    _plotHeat->setFixedWidth(_plotHeat->height() * 1.2);
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
#ifdef LOG_EX
    MethodLog _("recalcHeatmap");
#endif
    qDebug().nospace() << LOG_ID << " recalcHeatmap:"
        << " pointsX=" << _timelineX->data()->size() << ", pointsY=" << _timelineY->data()->size()
        << ", cellsX=" << _heatmapData->keySize() << ", cellsY=" << _heatmapData->valueSize()
        << ", rangeX=" << _valueMinX << ".." << _valueMaxX
        << ", rangeY=" << _valueMinY << ".." << _valueMaxY;

    const auto xs = _timelineX->data();
    const auto ys = _timelineY->data();
    if (xs->size() != ys->size()) {
        qCritical() << LOG_ID << "recalcHeatmap:"
            << "Unequal point count in timeline graphs" << xs->size() << ys->size();
        return;
    }
    const int pointCount = xs->size();
    if (pointCount == 0) return;
    
    if (_heatmapData->isEmpty())
        _heatmapData->setSize(_heatmapCellCount, _heatmapCellCount);

    int maxCellPoints = 0;
    const int cellCountX = _heatmapData->keySize();
    const int cellCountY = _heatmapData->valueSize();
    QCPRange rangeX(qMin(_valueMinX, -0.1), qMax(_valueMaxX, 0.1));
    QCPRange rangeY(qMin(_valueMinY, -0.1), qMax(_valueMaxY, 0.1));
    for (int i = 0; i < cellCountX; i++)
        for (int j = 0; j < cellCountY; j++)
            _heatmapData->setCell(i, j, qQNaN());
    _heatmapData->setRange(rangeX, rangeY);
    for (int i = 0; i < pointCount; i++) {
        int cellIndexX, cellIndexY;
        const double valueX = xs->at(i)->value;
        const double valueY = ys->at(i)->value;
        _heatmapData->coordToCell(valueX, valueY, &cellIndexX, &cellIndexY);
        if (cellIndexX < 0 || cellIndexX >= cellCountX) {
            qWarning().nospace() << LOG_ID << " recalcHeatmap:"
                << " Invalid cell index " << cellIndexX << " for X[" << i << "]=" << valueX
                << " (cells=" << cellCountX << ", range=" << rangeX.lower << ".." << rangeX.upper << ')';
            continue;
        }
        if (cellIndexY < 0 || cellIndexY >= cellCountY) {
            qWarning().nospace() << LOG_ID << " recalcHeatmap:"
                << " Invalid cell index " << cellIndexY << " for Y[" << i << "]=" << valueY
                << " (cells=" << cellCountY << ", range=" << rangeY.lower << ".." << rangeY.upper << ')';
            continue;
        }
        double cellPoints = _heatmapData->cell(cellIndexX, cellIndexY);
        if (qIsNaN(cellPoints)) cellPoints = 0;
        const int newCellPoints = int(cellPoints) + 1;
        if (newCellPoints > maxCellPoints) maxCellPoints = newCellPoints;
        _heatmapData->setCell(cellIndexX, cellIndexY, newCellPoints);
    }
    QCPRange rangeData(0, maxCellPoints);
    _heatmap->setDataRange(rangeData);
    _colorScale->setDataRange(rangeData);
    _plotHeat->xAxis->setRange(rangeX);
    _plotHeat->yAxis->setRange(rangeY);
}

void StabilityView::updateHeatmap(int lastPoints)
{
#ifdef LOG_EX
    MethodLog _("updateHeatmap");
    qDebug().nospace() << LOG_ID << " recalcHeatmap:"
        << " pointsX=" << _timelineX->data()->size() << ", pointsY=" << _timelineY->data()->size()
        << ", cellsX=" << _heatmapData->keySize() << ", cellsY=" << _heatmapData->valueSize()
        << ", rangeX=" << _valueMinX << ".." << _valueMaxX
        << ", rangeY=" << _valueMinY << ".." << _valueMaxY;
#endif

    const auto xs = _timelineX->data();
    const auto ys = _timelineY->data();
    if (xs->size() != ys->size()) {
        qCritical() << LOG_ID << "updateHeatmap:"
            << "Unequal point count in timeline graphs" << xs->size() << ys->size();
        return;
    }
    const int pointCount = xs->size();
    if (pointCount == 0) return;
    
    const int startIndex = pointCount - lastPoints;
    int maxCellPoints = _heatmap->dataRange().upper;
    const int cellCountX = _heatmapData->keySize();
    const int cellCountY = _heatmapData->valueSize();
    bool dataRangeChanged = false;
    for (int i = startIndex; i < pointCount; i++) {
        int cellIndexX, cellIndexY;
        const double valueX = xs->at(i)->value;
        const double valueY = ys->at(i)->value;
        _heatmapData->coordToCell(valueX, valueY, &cellIndexX, &cellIndexY);
        if (cellIndexX < 0 || cellIndexX >= cellCountX) {
            auto r = _heatmapData->keyRange();
            qWarning().nospace() << LOG_ID << " updateHeatmap:"
                << " Invalid cell index " << cellIndexX << " for X[" << i << "]=" << valueX
                << " (cells=" << cellCountX << ", range=" << r.lower << ".." << r.upper << ')';
            continue;
        }
        if (cellIndexY < 0 || cellIndexY >= cellCountY) {
            auto r = _heatmapData->valueRange();
            qWarning().nospace() << LOG_ID << " updateHeatmap:"
                << " Invalid cell index " << cellIndexY << " for Y[" << i << "]=" << valueY
                << " (cells=" << cellCountY << ", range=" << r.lower << ".." << r.upper << ')';
            continue;
        }
        double cellPoints = int(_heatmapData->cell(cellIndexX, cellIndexY));
        if (qIsNaN(cellPoints)) cellPoints = 0;
        const int newCellPoints = int(cellPoints) + 1;
        if (newCellPoints > maxCellPoints) maxCellPoints = newCellPoints, dataRangeChanged = true;
        _heatmapData->setCell(cellIndexX, cellIndexY, newCellPoints);
    }
    if (dataRangeChanged) {
        QCPRange rangeData(0, maxCellPoints);
        _heatmap->setDataRange(rangeData);
        _colorScale->setDataRange(rangeData);
    }
}

void StabilityView::rescaleTimeline(const PixelScale &newScale)
{
#ifdef LOG_EX
    MethodLog _("rescaleTimeline");
#endif
    qDebug().nospace() << LOG_ID << " rescaleTimeline:"
        << " old rangeX=" << _valueMinX << ".." <<  _valueMaxX
        << ", old rangeY=" << _valueMinY << ".." << _valueMaxY
        << ", scale "<< _scale.scaleFactor() << " -> " << newScale.scaleFactor();

    resetScale(false, true);

    QVector<QCPGraphData> &xs = _timelineX->data()->rawData();
    QVector<QCPGraphData> &ys = _timelineY->data()->rawData();
    if (xs.size() != ys.size()) {
        qCritical() << LOG_ID << "rescaleTimeline:"
            << "Unequal point count in timeline graphs" << xs.size() << ys.size();
        return;
    }
    const int pointCount = xs.size();
    if (pointCount == 0) return;

    for (int i = 0; i < pointCount; i++) {
        QCPGraphData &x = xs[i];
        QCPGraphData &y = ys[i];
        const double oldX = x.value;
        const double oldY = y.value;
        x.value = newScale.pixelToUnit(_scale.unitToPixel(oldX));
        y.value = newScale.pixelToUnit(_scale.unitToPixel(oldY));
        adjustValueRange(x.value, y.value);
        //qDebug() << "Rescale" << i << oldX << oldY << "->" << x.value << y.value;
    }

    qDebug().nospace() << LOG_ID << " rescaleTimeline:"
        << " new rangeX=" << _valueMinX << ".." <<  _valueMaxX
        << ", new rangeY=" << _valueMinY << ".." << _valueMaxY;
    
    adjustTimelineVertAxis();
}

bool StabilityView::adjustValueRange(double X, double Y)
{
    bool changed = false;

    const double max = qMax(X, Y);
    if (_valueMax < max || qIsNaN(_valueMax))
        _valueMax = max, changed = true;
    if (_valueMaxX < X || qIsNaN(_valueMaxX))
        _valueMaxX = X, changed = true;
    if (_valueMaxY < Y || qIsNaN(_valueMaxY))
        _valueMaxY = Y, changed = true;
    
    const double min = qMin(X, Y);
    if (_valueMin > min || qIsNaN(_valueMin))
        _valueMin = min, changed = true;
    if (_valueMinX > X || qIsNaN(_valueMinX))
        _valueMinX = X, changed = true;
    if (_valueMinY > Y || qIsNaN(_valueMinY))
        _valueMinY = Y, changed = true;

    return changed;
}

void StabilityView::adjustTimelineVertAxis()
{
    const double m = (_valueMax - _valueMin) * 0.1;
    _plotTime->yAxis->setRange(_valueMin - m, _valueMax + m);
}
