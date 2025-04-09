#include "ProfilesView.h"

#include "widgets/PlotIntf.h"

#include "beam_calc.h"

#include "helpers/OriLayouts.h"

#include "qcustomplot/src/core.h"
#include "qcustomplot/src/layoutelements/layoutelement-axisrect.h"
#include "qcustomplot/src/plottables/plottable-graph.h"

#include <cmath>

#define sqr(s) ((s)*(s))

struct CalcProfilesArg
{
    int imgW;
    int imgH;
    double *imgData;
    double profileRange;
    int pointCount;
    const PixelScale &scale;
};

static void calcProfiles(const CalcProfilesArg &arg, const CgnBeamResult &res, QVector<QCPGraphData>& gx, QVector<QCPGraphData>& gy)
{
    const int w = arg.imgW;
    const int h = arg.imgH;
    const double *data = arg.imgData;
    const int c = arg.pointCount - 1;
    const double step_x = res.dx / 2.0 * arg.profileRange / (double)(c);
    const double step_y = res.dy / 2.0 * arg.profileRange / (double)(c);
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

// Calculate Gaussian fit parameters from profile data
static void calcGaussFit(const QVector<QCPGraphData>& profile, QVector<QCPGraphData>& fit, double MI)
{
    double amplitude = 0;
    double center = 0;
    double width = 0.5; // Default fallback width

    const int cnt = profile.size();
    
    // Find the peak value and its position
    for (const auto& point : profile)
        if (point.value > amplitude) {
            amplitude = point.value;
            center = point.key;
        }
    
    // Estimate width by finding the half-maximum points
    double halfMax = amplitude / 2.0;
    double leftHalf = 0, rightHalf = 0;
    bool foundLeft = false, foundRight = false;
    
    // Find points closest to half maximum on both sides
    for (int i = 0; i < cnt - 1; ++i)
    {
        const auto &p = profile.at(i);
        const auto &p1 = profile.at(i+1);

        // Left side of peak
        if (!foundLeft && p.key < center && p.value <= halfMax && p1.value >= halfMax)
        {
            const double t = (halfMax - p.value) / (p1.value - p.value);
            leftHalf = p.key + t * (p1.key - p.key);
            foundLeft = true;
        }
        
        // Right side of peak
        if (!foundRight && p.key > center && p.value >= halfMax && p1.value <= halfMax)
        {
            const double t = (halfMax - p1.value) / (p.value - p1.value);
            rightHalf = p.key + t * (p1.key - p.key);
            foundRight = true;
        }
    }
    
    // Calculate width (FWHM)
    if (foundLeft && foundRight)
        width = rightHalf - leftHalf;

    // Convert FWHM to sigma (standard deviation)
    // For a Gaussian: FWHM = 2*sqrt(2*ln(2))*sigma ≈ 2.355*sigma
    double sigma = width / 2.355;

    // Apply M-square factor to the width (sigma)
    sigma *= sqrt(MI);

    for (int i = 0; i < cnt; ++i)
    {
        double x = profile.at(i).key;
        // Gaussian function: f(x) = A * exp(-(x-x0)²/(2*sigma²))
        fit[i].value = amplitude * exp(-sqr(x - center) / (2 * sqr(sigma)));
        fit[i].key = x;
    }
}

ProfilesView::ProfilesView(PlotIntf *plotIntf) : QWidget(), _plotIntf(plotIntf)
{
    _plotX = new QCustomPlot;
    _plotY = new QCustomPlot;

    _profileX = _plotX->addGraph();
    _profileY = _plotY->addGraph();
    _fitX = _plotX->addGraph();
    _fitY = _plotY->addGraph();
    
    QPen fitPen;
    fitPen.setColor(QColor(255, 0, 0));
    fitPen.setStyle(Qt::DashLine);
    _fitX->setPen(fitPen);
    _fitY->setPen(fitPen);

    QVector<QCPGraphData> data(2*_pointCount - 1);
    _profileX->data()->set(data);
    _profileY->data()->set(data);
    _fitX->data()->set(data);
    _fitY->data()->set(data);

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
        .profileRange = _profileRange,
        .pointCount = _pointCount,
        .scale = _scale,
    };

    auto results = _plotIntf->results();
    if (results.empty()) return;
    const auto& res = results.first();

    auto& profileX = _profileX->data()->rawData();
    auto& profileY = _profileY->data()->rawData();

    calcProfiles(arg, res, profileX, profileY);
    if (_showFit) {
        calcGaussFit(profileX, _fitX->data()->rawData(), _MI);
        calcGaussFit(profileY, _fitY->data()->rawData(), _MI);
    }

    double min = _plotIntf->rangeMin();
    double max = _plotIntf->rangeMax();
    _plotX->yAxis->setRange(min, max);
    _plotY->yAxis->setRange(min, max);
    double rangeX = _scale.pixelToUnit(res.dx /2.0 * _profileRange);
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
