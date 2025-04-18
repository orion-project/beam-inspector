#include "ProfilesView.h"

#include "app/HelpSystem.h"
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

#include <QTimer>

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

struct GaussParams
{
    double width;
    double center;
    double amplitude;
};

// Calculate Gaussian fit parameters from profile data
static void calcGaussParams(const QVector<QCPGraphData>& profile, GaussParams &gp)
{
    gp.width = 0.5; // Default fallback width
    gp.center = 0;
    gp.amplitude = 0;

    const int cnt = profile.size();

    // Find the peak value and its position
    for (const auto& point : profile)
        if (point.value > gp.amplitude) {
            gp.amplitude = point.value;
            gp.center = point.key;
        }

    // Estimate width by finding the half-maximum points
    double halfMax = gp.amplitude / 2.0;
    double leftHalf = 0, rightHalf = 0;
    bool foundLeft = false, foundRight = false;

    // Find points closest to half maximum on both sides
    for (int i = 0; i < cnt - 1; ++i)
    {
        const auto &p = profile.at(i);
        const auto &p1 = profile.at(i+1);

        // Left side of peak
        if (!foundLeft && p.key < gp.center && p.value <= halfMax && p1.value >= halfMax)
        {
            const double t = (halfMax - p.value) / (p1.value - p.value);
            leftHalf = p.key + t * (p1.key - p.key);
            foundLeft = true;
        }

        // Right side of peak
        if (!foundRight && p.key > gp.center && p.value >= halfMax && p1.value <= halfMax)
        {
            const double t = (halfMax - p1.value) / (p.value - p1.value);
            rightHalf = p.key + t * (p1.key - p.key);
            foundRight = true;
        }
    }

    // Calculate width (FWHM)
    if (foundLeft && foundRight)
        gp.width = rightHalf - leftHalf;
}

// Calculate Gaussian fit parameters from profile data
static double calcGaussFit(const QVector<QCPGraphData>& profile, QVector<QCPGraphData>& fit, const GaussParams &gp, double MI, bool center)
{
    // Convert FWHM to sigma (standard deviation)
    // For a Gaussian: FWHM = 2*sqrt(2*ln(2))*sigma ≈ 2.355*sigma
    double sigma = gp.width / 2.3548200450309493;

    // Apply M-square factor to the width (sigma)
    sigma *= sqrt(MI);

    const int cnt = profile.size();

    for (int i = 0; i < cnt; ++i)
    {
        double x = profile.at(i).key;
        // Gaussian function: f(x) = A * exp(-(x-x0)²/(2*sigma²))
        fit[i].value = gp.amplitude * exp(-sqr(x - gp.center) / (2 * sqr(sigma)));
        fit[i].key = x - (center ? gp.center : 0);
    }

    // Gaussian beam: I = exp(-2*r²/w²) [https://en.wikipedia.org/wiki/Gaussian_beam]
    // Gaussiam func: F = exp(-r²/(2σ²)) [https://en.wikipedia.org/wiki/Gaussian_function]
    // => beam w = 2*σ - 1/e beam radius
    // We return diameter for easier comparison with results table
    return sigma * 4.0;
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

    auto actnSep2 = new QAction(this);
    actnSep2->setSeparator(true);

    auto actnSep3 = new QAction(this);
    actnSep3->setSeparator(true);

    _actnShowFit = A_(tr("Show Gaussian Fit"), this, &ProfilesView::toggleShowFit, ":/toolbar/profile");
    _actnShowFit->setCheckable(true);

    _actnCopyFitX = A_(tr("Copy Fitted Points"), this, [this]{ copyGraph(_fitX); });
    _actnCopyFitY = A_(tr("Copy Fitted Points"), this, [this]{ copyGraph(_fitY); });

    _actnSetMI = A_(tr("Set Beam Quality (M²)..."), this, &ProfilesView::setMI, ":/toolbar/mi");

    _actnCenterFit = A_(tr("Center Fit at Zero"), this, &ProfilesView::toggleCenterFit);
    _actnCenterFit->setCheckable(true);

    auto actnAutoScale = A_(tr("Autoscale Plots"), this, &ProfilesView::autoScale);

    _actnShowFullY = A_(tr("Show Full Range"), this, &ProfilesView::toggleShowFullY);
    _actnShowFullY->setCheckable(true);

    auto actnHelp = A_(tr("Help"), this, []{ HelpSystem::topic("profiles_view"); }, ":/ori_images/help");

    _plotX->addAction(_actnShowFit);
    _plotX->addAction(_actnCenterFit);
    _plotX->addAction(_actnSetMI);
    _plotX->addAction(actnSep1);
    _plotX->addAction(A_(tr("Copy Profile Points"), this, [this]{ copyGraph(_profileX); }, ":/toolbar/copy"));
    _plotX->addAction(_actnCopyFitX);
    _plotX->addAction(A_(tr("Copy as Image"), this, [this]{ copyImage(_plotX); }, ":/toolbar/copy_img"));
    _plotX->addAction(actnSep2);
    //_plotX->addAction(_actnShowFullY);
    _plotX->addAction(actnAutoScale);
    _plotX->addAction(actnSep3);
    _plotX->addAction(actnHelp);

    _plotY->addAction(_actnShowFit);
    _plotY->addAction(_actnCenterFit);
    _plotY->addAction(_actnSetMI);
    _plotY->addAction(actnSep1);
    _plotY->addAction(A_(tr("Copy Profile Points"), this, [this]{ copyGraph(_profileY); }, ":/toolbar/copy"));
    _plotY->addAction(_actnCopyFitY);
    _plotY->addAction(A_(tr("Copy as Image"), this, [this]{ copyImage(_plotY); }, ":/toolbar/copy_img"));
    _plotY->addAction(actnSep2);
    //_plotY->addAction(_actnShowFullY);
    _plotY->addAction(actnAutoScale);
    _plotY->addAction(actnSep3);
    _plotY->addAction(actnHelp);
}

void ProfilesView::setThemeColors(PlotHelpers::Theme theme, bool replot)
{
    PlotHelpers::setThemeColors(_plotX, theme);
    PlotHelpers::setThemeColors(_plotY, theme);
    _textMiX->setTextColor(PlotHelpers::themeAxisLabelColor(theme));
    _textMiY->setTextColor(PlotHelpers::themeAxisLabelColor(theme));
    QColor profileColor = PlotHelpers::isDarkTheme(theme) ? QColor(30, 144, 255) : QColor(Qt::blue);
    _profileX->setPen(QPen(profileColor));
    _profileY->setPen(QPen(profileColor));
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

    GaussParams gpX, gpY;
    calcGaussParams(profileX, gpX);
    calcGaussParams(profileY, gpY);
    _profileX->setName(QStringLiteral("X (FWHM: %1)").arg(_scale.format(gpX.width)));
    _profileY->setName(QStringLiteral("Y (FWHM: %1)").arg(_scale.format(gpY.width)));

    if (_showFit) {
        const double wx = calcGaussFit(profileX, _fitX->data()->rawData(), gpX, _MI, _centerFit);
        const double wy = calcGaussFit(profileY, _fitY->data()->rawData(), gpY, _MI, _centerFit);
        _fitX->setName(QStringLiteral("Fit (E2FW: %1)").arg(_scale.format(wx)));
        _fitY->setName(QStringLiteral("Fit (E2FW: %1)").arg(_scale.format(wy)));
    }

    double rangeY = _showFullY ? _plotIntf->rangeMax() : qMax(gpX.amplitude, gpY.amplitude);
    if (rangeY > _rangeY) {
        _rangeY = rangeY;
        _plotX->yAxis->setRange(0, _rangeY);
        _plotY->yAxis->setRange(0, _rangeY);
    }
    double rangeX = qMax(
        _scale.pixelToUnit(res.dx /2.0 * _profileRange),
        _scale.pixelToUnit(res.dy /2.0 * _profileRange)
    );
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
    _rangeY = 0;
}

void ProfilesView::setScale(const PixelScale& scale)
{
    _scale = scale;
    QTimer::singleShot(1000, [this]{
        _rangeX = 0;
        _rangeY = 0;
    });
}

void ProfilesView::updateVisibility()
{
    _actnShowFit->setChecked(_showFit);
    _fitX->setVisible(_showFit);
    _fitY->setVisible(_showFit);
    _actnCopyFitX->setVisible(_showFit);
    _actnCopyFitY->setVisible(_showFit);
    _actnCenterFit->setVisible(_showFit);
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
    _centerFit = s->value("centerFit", false).toBool();
    //_showFullY = s->value("showFullY", false).toBool();
    _showFullY = true;
    _MI = s->value("MI", 1).toDouble();
    s->endGroup();
    updateVisibility();
    _actnCenterFit->setChecked(_centerFit);
    _actnShowFullY->setChecked(_showFullY);
}

void ProfilesView::storeState()
{
    Ori::Settings s;
    s.beginGroup("Profiles");
    s.setValue("showFit", _showFit);
    s.setValue("centerFit", _centerFit);
    s.setValue("showFullY", _showFullY);
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

void ProfilesView::toggleCenterFit()
{
    _centerFit = !_centerFit;
    _actnCenterFit->setChecked(_centerFit);
    storeState();
    showResult();
}

void ProfilesView::toggleShowFullY()
{
    _showFullY = !_showFullY;
    _actnShowFullY->setChecked(_showFullY);
    _rangeY = 0;
    storeState();
    showResult();
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
    editor.selectAll();

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

void ProfilesView::autoScale()
{
    _rangeX = 0;
    _rangeY = 0;
    showResult();
}
