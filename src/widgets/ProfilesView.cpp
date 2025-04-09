#include "ProfilesView.h"

#include "widgets/PlotIntf.h"

#include "beam_calc.h"

#include "helpers/OriLayouts.h"

#include "qcustomplot/src/core.h"
#include "qcustomplot/src/layoutelements/layoutelement-axisrect.h"
#include "qcustomplot/src/plottables/plottable-graph.h"

struct CalcProfilesArg
{
    int imgW;
    int imgH;
    double *imgData;
    double profileWidth;
    int pointCount;
    const PixelScale &scale;
};

template <typename T>
void calcProfiles(const CalcProfilesArg &arg, const CgnBeamResult &res, QVector<T>& gx, QVector<T>& gy)
{
    const int w = arg.imgW;
    const int h = arg.imgH;
    const double *data = arg.imgData;
    const int c = arg.pointCount - 1;
    const double step_x = res.dx / 2.0 * arg.profileWidth / (double)(c);
    const double step_y = res.dy / 2.0 * arg.profileWidth / (double)(c);
    const double a = res.phi / 57.29577951308232;
    const double sin_a = sin(a);
    const double cos_a = cos(a);
    const int x0 = round(res.xc);
    const int y0 = round(res.yc);
    int xi, yi, dx, dy;
    double r, p;
    for (int i = 0; i < arg.pointCount; i++) {
        // along X-width
        r = step_x * (double)i;
        dx = round(r * cos_a);
        dy = round(r * sin_a);

        // positive half
        xi = x0 + dx;
        yi = y0 + dy;
        p = (xi >= 0 && xi < w && yi >= 0 && yi < h) ? data[w * yi + xi] : 0;
        gx[c + i].key = arg.scale.pixelToUnit(r);
        gx[c + i].value = p;
        //qDebug() << "+x" << i <<  xi << yi << r << p;

        // negative half
        xi = x0 - dx;
        yi = y0 - dy;
        p = (xi >= 0 && xi < w && yi >= 0 && yi < h) ? data[w * yi + xi] : 0;
        gx[c - i].key = arg.scale.pixelToUnit(-r);
        gx[c - i].value = p;
        //qDebug() << "-x" << i << xi << yi << r << p;

        // along Y-width
        r = step_y * (double)i;
        dx = round(r * sin_a);
        dy = round(r * cos_a);

        // positive half
        xi = x0 + dx;
        yi = y0 + dy;
        p = (xi >= 0 && xi < w && yi >= 0 && yi < h) ? data[w * yi + xi] : 0;
        gy[c + i].key = arg.scale.pixelToUnit(r);
        gy[c + i].value = p;
        //qDebug() << "+y" << i << xi << yi << r << p;

        // negative half
        r = step_y * (double)i;
        xi = x0 - dx;
        yi = y0 - dy;
        p = (xi >= 0 && xi < w && yi >= 0 && yi < h) ? data[w * yi + xi] : 0;
        gy[c - i].key = arg.scale.pixelToUnit(-r);
        gy[c - i].value = p;
        //qDebug() << "-y" << i << xi << yi << r << p;
    }
}

ProfilesView::ProfilesView(PlotIntf *plotIntf) : QWidget(), _plotIntf(plotIntf)
{
    _plotX = new QCustomPlot;
    _plotY = new QCustomPlot;

    _graphX = _plotX->addGraph();
    _graphY = _plotY->addGraph();

    QVector<QCPGraphData> data(2*_pointCount - 1);
    _graphX->data()->set(data);
    _graphY->data()->set(data);

    setThemeColors(PlotHelpers::SYSTEM, false);

    Ori::Layouts::LayoutH({_plotX, _plotY}).setMargins(12, 0, 12, 0).useFor(this);
}

void ProfilesView::setThemeColors(PlotHelpers::Theme theme, bool replot)
{
    PlotHelpers::setThemeColors(_plotX, theme);
    PlotHelpers::setThemeColors(_plotY, theme);
    if (replot) {
        _plotX->replot();
        _plotY->replot();
    }
}

void ProfilesView::showResult()
{
    CalcProfilesArg arg {
        .imgW = _plotIntf->graphW(),
        .imgH = _plotIntf->graphH(),
        .imgData = _plotIntf->rawGraph(),
        .profileWidth = _profileWidth,
        .pointCount = _pointCount,
        .scale = _scale,
    };

    auto results = _plotIntf->results();
    if (results.empty()) return;
    const auto& res = results.first();

    calcProfiles(arg, res, _graphX->data()->rawData(), _graphY->data()->rawData());

    double min = _plotIntf->rangeMin();
    double max = _plotIntf->rangeMax();
    _plotX->yAxis->setRange(min, max);
    _plotY->yAxis->setRange(min, max);
    double rangeX = _scale.pixelToUnit(res.dx /2.0 * _profileWidth);
    if (rangeX > _rangeX) {
        _rangeX = rangeX;
        _plotX->xAxis->setRange(-_rangeX, _rangeX);
        _plotY->xAxis->setRange(-_rangeX, _rangeX);
    }
    _plotX->replot();
    _plotY->replot();
}

void ProfilesView::cleanResult()
{
    _rangeX = 0;
}

void ProfilesView::setScale(const PixelScale& scale)
{
    _scale = scale;
    _rangeX = 0;
}
