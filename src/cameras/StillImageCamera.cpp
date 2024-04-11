#include "StillImageCamera.h"

#include "widgets/PlotIntf.h"
#include "widgets/TableIntf.h"

#include "beam_calc.h"
#include "beam_render.h"

#include "tools/OriSettings.h"
#include "helpers/OriDialogs.h"

#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QMessageBox>

#define SETTINGS_KEY "StillImageCamera"

bool StillImageCamera::editSettings()
{
    return CameraSettings::editDlg(SETTINGS_KEY);
}

CameraSettings StillImageCamera::loadSettings()
{
    auto settings = CameraSettings();
    settings.load(SETTINGS_KEY);
    //settings.print();
    return settings;
}

std::optional<CameraInfo> StillImageCamera::start(PlotIntf *plot, TableIntf *table)
{
    Ori::Settings s;
    s.beginGroup(SETTINGS_KEY);

    QString fileName = QFileDialog::getOpenFileName(qApp->activeWindow(),
                                qApp->tr("Open Beam Image"),
                                s.strValue("recentDir"),
                                qApp->tr("Images (*.png *.pgm *.jpg);;All Files (*.*)"));
    if (fileName.isEmpty())
        return {};

    s.setValue("recentDir", QFileInfo(fileName).dir().absolutePath());

    return start(fileName, plot, table);
}

std::optional<CameraInfo> StillImageCamera::start(const QString& fileName, PlotIntf *plot, TableIntf *table)
{
    auto settings = loadSettings();

    QElapsedTimer timer;

    timer.start();
    QImage img(fileName);
    if (img.isNull()) {
        Ori::Dlg::error(qApp->tr("Unable to to load image file"));
        return {};
    }
    auto loadTime = timer.elapsed();

    auto fmt = img.format();
    if (fmt != QImage::Format_Grayscale8 && fmt != QImage::Format_Grayscale16) {
        Ori::Dlg::error(qApp->tr("Wrong image format, only grayscale images are supported"));
        return {};
    }

    plot->initGraph(img.width(), img.height());

    // declare explicitly as const to avoid deep copy
    const uchar* buf = img.bits();

    CgnBeamCalc c;
    c.w = img.width();
    c.h = img.height();
    c.bits = fmt == QImage::Format_Grayscale8 ? 8 : 16;
    c.buf = (uint8_t*)buf;

    int sz = c.w*c.h;
    double *graph = plot->rawGraph();

    CgnBeamResult r;
    r.x1 = 0, r.x2 = c.w;
    r.y1 = 0, r.y2 = c.h;

    CgnBeamBkgnd g;
    g.iters = 0;
    g.max_iter = settings.maxIters;
    g.precision = settings.precision;
    g.corner_fraction = settings.cornerFraction;
    g.nT = settings.nT;
    g.mask_diam = settings.maskDiam;
    g.min = 0;
    g.max = 0;
    QVector<double> subtracted;
    if (settings.subtractBackground) {
        subtracted = QVector<double>(sz);
        g.subtracted = subtracted.data();
    }

    timer.restart();
    if (settings.subtractBackground) {
        cgn_calc_beam_bkgnd(&c, &g, &r);
    } else {
        cgn_calc_beam_naive(&c, &r);
    }
    auto calcTime = timer.elapsed();

    timer.restart();
    if (settings.subtractBackground) {
        if (settings.normalize) {
            cgn_copy_normalized_f46(g.subtracted, graph, sz, g.min, g.max);
        } else {
            memcpy(plot, g.subtracted, sizeof(double)*sz);
        }
    } else {
        if (settings.normalize) {
            if (c.bits > 8) {
                cgn_render_beam_to_doubles_norm_16((const uint16_t*)c.buf, sz, graph);
            } else {
                cgn_render_beam_to_doubles_norm_8((const uint8_t*)c.buf, sz, graph);
            }
        } else {
            cgn_copy_to_f64(&c, graph, &g.max);
        }
    }
    auto copyTime = timer.elapsed();

    qDebug() << "loadTime:" << loadTime << "calcTime:" << calcTime << "copyTime:" << copyTime << "iters:" << g.iters;

    plot->invalidateGraph();
    if (settings.normalize)
        plot->setResult(r, 0, 1);
    else plot->setResult(r, g.min, g.max);
    table->setResult(r, loadTime, calcTime);

    QFileInfo fi(fileName);
    return CameraInfo {
        .name = fi.fileName(),
        .descr = fi.absoluteFilePath(),
        .width = img.width(),
        .height = img.height(),
        .bits = img.depth(),
    };
}
