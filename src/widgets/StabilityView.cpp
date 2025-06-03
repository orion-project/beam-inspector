#include "StabilityView.h"

#include "helpers/OriLayouts.h"
#include "helpers/OriWidgets.h"
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
#define DISPLAY_RANGE_S 300
#define DATA_BUF_SIZE 10

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

    // By default, the legend is in the inset layout of the main axis rect.
    // So this is how we access it to change legend placement:
    _plotTime->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignLeft|Qt::AlignTop);
    
    QSharedPointer<QCPAxisTickerDateTime> timeTicker(new QCPAxisTickerDateTime);
    timeTicker->setDateTimeSpec(Qt::LocalTime);
    timeTicker->setDateTimeFormat("hh:mm:ss");
    _plotTime->xAxis->setTicker(timeTicker);
    
    _timelineX = _plotTime->addGraph();
    _timelineY = _plotTime->addGraph();
    _timelineX->setName("Center X");
    _timelineY->setName("Center Y");
    
    _timelineLen = new QCPTextElement(_plotTime);
    
    _plotTime->axisRect()->insetLayout()->addElement(_timelineLen, Qt::AlignRight|Qt::AlignTop);
    
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
    
    Ori::Layouts::LayoutH({_plotTime, _plotHeat}).setMargins(12, 0, 12, 0).useFor(this);
}

void StabilityView::setThemeColors(PlotHelpers::Theme theme, bool replot)
{
    PlotHelpers::setThemeColors(_plotTime, theme);
    PlotHelpers::setThemeColors(_plotHeat, theme);
    _timelineLen->setTextColor(PlotHelpers::themeAxisLabelColor(theme));
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

void StabilityView::setConfig(const PixelScale& scale)
{
    _scale = scale;

    QTimer::singleShot(1000, this, [this]{
        resetScale(false, true);
    });
}

void StabilityView::setResult(qint64 time, const QList<CgnBeamResult>& val)
{
    if (val.isEmpty())
        return;

    // Only first ROI
    const auto &r = val.first();
    if (r.nan)
        return;
        
    if (_dataBufCursor >= DATA_BUF_SIZE)
        return;
        
    _dataBuf[_dataBufCursor] = DataPoint {
        .time = time,
        .x = r.xc,
        .y = r.yc,
    };
    _dataBufCursor++;
    _frameTime = time;
    if (_startTime < 0)
        _startTime = _frameTime;
}

void StabilityView::showResult()
{
    if (!(_showTime < 0 || _frameTime - _showTime >= UPDATE_INTERVAL_MS))
        return;
        
    int newPoints = _dataBufCursor;
    bool timelineRangeChangedV = false;
    for (int i = 0; i < newPoints; i++) {
        const auto &p = _dataBuf.at(i);
        const double time = p.time / 1000.0;
        
        if (_offsetX < 0) _offsetX = p.x;
        if (_offsetY < 0) _offsetY = p.y;

        const double timelineX = _scale.pixelToUnit(p.x - _offsetX);
        const double timelineY = _scale.pixelToUnit(p.y - _offsetY);

        _timelineX->addData(time, timelineX);
        _timelineY->addData(time, timelineY);

        const double timelineMaxV = qMax(timelineX, timelineY);
        if (_timelineMaxV < timelineMaxV || qIsNaN(_timelineMaxV))
            _timelineMaxV = timelineMaxV, timelineRangeChangedV = true;
        
        const double timelineMinV = qMin(timelineX, timelineY);
        if (_timelineMinV > timelineMinV || qIsNaN(_timelineMinV))
            _timelineMinV = timelineMinV, timelineRangeChangedV = true;

        _timelineMaxK = time;
        if (_timelineMinK < 0)
            _timelineMinK = time;
    }

    if (timelineRangeChangedV) {
        const double m = (_timelineMaxV - _timelineMinV) * 0.1;
        _plotTime->yAxis->setRange(_timelineMinV - m, _timelineMaxV + m);
    }
    
    if (_cleanTime < 0) {
        _cleanTime = _frameTime;
    } else if (_frameTime - _cleanTime >= CLEAN_INTERVAL_MS) {
        if (_timelineMaxK - _timelineMinK > DISPLAY_RANGE_S) {
            _timelineMinK = _timelineMaxK - DISPLAY_RANGE_S;
            _timelineX->data()->removeBefore(_timelineMinK);
            _timelineY->data()->removeBefore(_timelineMinK);
            _cleanTime = _frameTime;
        }
    } 
    _plotTime->xAxis->setRange(timelineDisplayMinK(), _timelineMaxK);

    _plotTime->replot();
    _plotHeat->replot();
    
    _timelineLen->setText(formatSecs((_frameTime - _startTime) / 1000));
    
    //qDebug() << _timelineX->dataCount() << _plotTime->replotTime(true);
    
    _showTime = _frameTime;
    _dataBufCursor = 0;
}

double StabilityView::timelineDisplayMinK() const
{
    return qMax(_timelineMinK, _timelineMaxK-DISPLAY_RANGE_S);
}

void StabilityView::cleanResult()
{
    _timelineX->data().clear();
    _timelineY->data().clear();
    _frameTime = -1;
    _showTime = -1;
    _cleanTime = -1;
    _startTime = -1;
    _offsetX = -1;
    _offsetY = -1;
    _dataBufCursor = 0;
    _timelineLen->setText(QString());
    resetScale(true, true);
}

void StabilityView::resetScale(bool key, bool value)
{
    if (key) {
        _timelineMinK = -1;
        _timelineMaxK = -1;
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
    double startKey = timelineDisplayMinK();
    foreach (const auto& p, graph->data()->rawData()) {
        if (p.key < startKey)
            continue;
        QString timeStr = QDateTime::fromSecsSinceEpoch(p.key).toString(Qt::ISODateWithMs);
        s << timeStr << ',' << QString::number(p.value) << '\n';
    }
    qApp->clipboard()->setText(res);
    Ori::Gui::PopupMessage::affirm(qApp->tr("Data points been copied to Clipboard"), Qt::AlignRight|Qt::AlignBottom);
}
