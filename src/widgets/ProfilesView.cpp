#include "ProfilesView.h"

#include "widgets/PlotIntf.h"

#include "beam_calc.h"

#include "helpers/OriDialogs.h"
#include "helpers/OriLayouts.h"
#include "helpers/OriWidgets.h"
#include "tools/OriSettings.h"
#include "widgets/OriPopupMessage.h"
#include "widgets/OriValueEdit.h"

#include "qcustomplot/src/core.h"
#include "qcustomplot/src/layoutelements/layoutelement-axisrect.h"
#include "qcustomplot/src/layoutelements/layoutelement-legend.h"
#include "qcustomplot/src/layoutelements/layoutelement-textelement.h"
#include "qcustomplot/src/plottables/plottable-graph.h"

#include <cmath>

#define sqr(s) ((s)*(s))

#define A_ Ori::Gui::action

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
static double calcGaussFit(const QVector<QCPGraphData>& profile, QVector<QCPGraphData>& fit, double MI)
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

    return width;
}

ProfilesView::ProfilesView(PlotIntf *plotIntf) : QWidget(), _plotIntf(plotIntf)
{
    _plotX = new QCustomPlot;
    _plotY = new QCustomPlot;

    _plotX->setContextMenuPolicy(Qt::ActionsContextMenu);
    _plotY->setContextMenuPolicy(Qt::ActionsContextMenu);

    _plotX->legend->setVisible(true);
    _plotY->legend->setVisible(true);

    // By default, the legend is in the inset layout of the main axis rect.
    // So this is how we access it to change legend placement:
    _plotX->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignLeft|Qt::AlignTop);
    _plotY->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignLeft|Qt::AlignTop);

    _textMiX = new QCPTextElement(_plotX);
    _textMiY = new QCPTextElement(_plotY);
    connect(_textMiX, &QCPTextElement::doubleClicked, this, &ProfilesView::setMI);
    connect(_textMiY, &QCPTextElement::doubleClicked, this, &ProfilesView::setMI);

    _plotX->axisRect()->insetLayout()->addElement(_textMiX, Qt::AlignRight|Qt::AlignTop);
    _plotY->axisRect()->insetLayout()->addElement(_textMiY, Qt::AlignRight|Qt::AlignTop);

    _profileX = _plotX->addGraph();
    _profileY = _plotY->addGraph();
    _profileX->setName("X");
    _profileY->setName("Y");

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

    auto actnSep1 = new QAction(this);
    actnSep1->setSeparator(true);

    _actnShowFit = A_(tr("Show Gaussian Fit"), this, &ProfilesView::toggleShowFit, ":/toolbar/profile");
    _actnShowFit->setCheckable(true);

    _actnCopyFitX = A_(tr("Copy Fitted Points"), this, [this]{ copyGraph(_fitX); }, ":/toolbar/copy");
    _actnCopyFitY = A_(tr("Copy Fitted Points"), this, [this]{ copyGraph(_fitY); }, ":/toolbar/copy");

    _actnSetMI = A_(tr("Set beam quality (M²)..."), this, &ProfilesView::setMI, ":/toolbar/mi");

    _plotX->addAction(A_(tr("Copy Profile Points"), this, [this]{ copyGraph(_profileX); }, ":/toolbar/copy"));
    _plotX->addAction(_actnCopyFitX);
    _plotX->addAction(A_(tr("Copy as Image"), this, [this]{ copyImage(_plotX); }, ":/toolbar/copy_img"));
    _plotX->addAction(actnSep1);
    _plotX->addAction(_actnShowFit);
    _plotX->addAction(_actnSetMI);

    _plotY->addAction(A_(tr("Copy Profile Points"), this, [this]{ copyGraph(_profileY); }, ":/toolbar/copy"));
    _plotY->addAction(_actnCopyFitY);
    _plotY->addAction(A_(tr("Copy as Image"), this, [this]{ copyImage(_plotY); }, ":/toolbar/copy_img"));
    _plotY->addAction(actnSep1);
    _plotY->addAction(_actnShowFit);
    _plotY->addAction(_actnSetMI);
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
        double wx = calcGaussFit(profileX, _fitX->data()->rawData(), _MI);
        double wy = calcGaussFit(profileY, _fitY->data()->rawData(), _MI);
        _fitX->setName(QStringLiteral("FWHM: ") + _scale.format(wx));
        _fitY->setName(QStringLiteral("FWHM: ") + _scale.format(wy));
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

void ProfilesView::updateVisibility()
{
    _actnShowFit->setChecked(_showFit);
    _fitX->setVisible(_showFit);
    _fitY->setVisible(_showFit);
    _actnCopyFitX->setVisible(_showFit);
    _actnCopyFitY->setVisible(_showFit);
    _actnSetMI->setVisible(_showFit);
    _textMiX->setVisible(_showFit);
    _textMiY->setVisible(_showFit);
    if (_showFit) {
        _fitX->addToLegend();
        _fitY->addToLegend();
        showMI();
    } else {
        _fitX->removeFromLegend();
        _fitY->removeFromLegend();
    }
}

void ProfilesView::restoreState(QSettings *s)
{
    s->beginGroup("Profiles");
    _showFit = s->value("showFit", true).toBool();
    _MI = s->value("MI", 1).toDouble();
    s->endGroup();
    updateVisibility();
}

void ProfilesView::storeState()
{
    Ori::Settings s;
    s.beginGroup("Profiles");
    s.setValue("showFit", _showFit);
    s.setValue("MI", _MI);
}

void ProfilesView::toggleShowFit()
{
    _showFit = !_showFit;
    updateVisibility();
    storeState();
    if (_showFit)
        showResult();
    else {
        _plotX->replot();
        _plotY->replot();
    }
}

void ProfilesView::copyImage(QCustomPlot *plot)
{
    QImage image(plot->width(), plot->height(), QImage::Format_RGB32);
    QCPPainter painter(&image);
    setThemeColors(PlotHelpers::LIGHT, false);
    plot->toPainter(&painter);
    setThemeColors(PlotHelpers::SYSTEM, false);
    qApp->clipboard()->setImage(image);

    Ori::Gui::PopupMessage::affirm(tr("Image has been copied to Clipboard"), Qt::AlignRight|Qt::AlignBottom);
}

void ProfilesView::copyGraph(QCPGraph *graph)
{
    QString res;
    QTextStream s(&res);
    foreach (const auto& p, graph->data()->rawData()) {
        s << QString::number(p.key) << ',' << QString::number(p.value) << '\n';
    }
    qApp->clipboard()->setText(res);

    Ori::Gui::PopupMessage::affirm(tr("Data points been copied to Clipboard"), Qt::AlignRight|Qt::AlignBottom);
}

void ProfilesView::setMI()
{
    Ori::Widgets::ValueEdit editor;
    Ori::Gui::adjustFont(&editor);
    editor.setValue(_MI);

    if (Ori::Dlg::showDialogWithPromptV("Beam quality parameter (M²)", &editor)) {
        double mi = editor.value();
        if (mi <= 0) mi = 1;
        _MI = mi;
        storeState();
        showMI();
        showResult();
    }
}

void ProfilesView::showMI()
{
    QString s = QStringLiteral("M² = %1").arg(_MI);
    _textMiX->setText(s);
    _textMiY->setText(s);
}
