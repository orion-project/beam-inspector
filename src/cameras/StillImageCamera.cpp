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

StillImageCamera::StillImageCamera(PlotIntf *plot, TableIntf *table) : Camera(plot, table, "StillImageCamera")
{
    Ori::Settings s;
    s.beginGroup(_configGroup);

    QString fileName = QFileDialog::getOpenFileName(qApp->activeWindow(),
                                qApp->tr("Open Beam Image"),
                                s.strValue("recentDir"),
                                qApp->tr("Images (*.png *.pgm *.jpg);;All Files (*.*)"));
    if (!fileName.isEmpty()) {
        _fileName = fileName;
        s.setValue("recentDir", QFileInfo(fileName).dir().absolutePath());
    }
}

StillImageCamera::StillImageCamera(PlotIntf *plot, TableIntf *table, const QString& fileName) :
    Camera(plot, table, "StillImageCamera"), _fileName(fileName)
{
}

QString StillImageCamera::name() const
{
    QFileInfo fi(_fileName);
    return fi.fileName();
}

QString StillImageCamera::descr() const
{
    QFileInfo fi(_fileName);
    return fi.absoluteFilePath();
}

int StillImageCamera::width() const
{
    return _image.width();
}

int StillImageCamera::height() const
{
    return _image.height();
}

int StillImageCamera::bits() const
{
    return _image.depth();
}

void StillImageCamera::capture()
{
    QElapsedTimer timer;

    timer.start();
    _image = QImage(_fileName);
    if (_image.isNull()) {
        Ori::Dlg::error(qApp->tr("Unable to to load image file"));
        return;
    }
    auto loadTime = timer.elapsed();

    auto fmt = _image.format();
    if (fmt != QImage::Format_Grayscale8 && fmt != QImage::Format_Grayscale16) {
        Ori::Dlg::error(qApp->tr("Wrong image format, only grayscale images are supported"));
        return;
    }

    // declare explicitly as const to avoid deep copy
    const uchar* buf = _image.bits();

    CgnBeamCalc c;
    c.w = _image.width();
    c.h = _image.height();
    c.bits = fmt == QImage::Format_Grayscale8 ? 8 : 16;
    c.buf = (uint8_t*)buf;

    int sz = c.w*c.h;

    _plot->initGraph(c.w, c.h);
    double *graph = _plot->rawGraph();

    CgnBeamResult r;

    CgnBeamBkgnd g;
    g.iters = 0;
    g.max_iter = _config.bgnd.iters;
    g.precision = _config.bgnd.precision;
    g.corner_fraction = _config.bgnd.corner;
    g.nT = _config.bgnd.noise;
    g.mask_diam = _config.bgnd.mask;
    g.min = 0;
    g.max = 0;
    if (_config.aperture.on && _config.aperture.isValid(c.w, c.h)) {
        g.ax1 = _config.aperture.x1;
        g.ay1 = _config.aperture.y1;
        g.ax2 = _config.aperture.x2;
        g.ay2 = _config.aperture.y2;
        r.x1 = _config.aperture.x1;
        r.y1 = _config.aperture.y1;
        r.x2 = _config.aperture.x2;
        r.y2 = _config.aperture.y2;
    } else {
        g.ax1 = 0;
        g.ay1 = 0;
        g.ax2 = c.w;
        g.ay2 = c.h;
        r.x1 = 0;
        r.y1 = 0;
        r.x2 = c.w;
        r.y2 = c.h;
    }
    QVector<double> subtracted;
    if (_config.bgnd.on) {
        subtracted = QVector<double>(sz);
        g.subtracted = subtracted.data();
    }

    timer.restart();
    if (_config.bgnd.on) {
        cgn_calc_beam_bkgnd(&c, &g, &r);
    } else {
        cgn_calc_beam_naive(&c, &r);
    }
    auto calcTime = timer.elapsed();

    timer.restart();
    if (_config.bgnd.on) {
        if (_config.normalize) {
            cgn_copy_normalized_f64(g.subtracted, graph, sz, g.min, g.max);
        } else {
            memcpy(graph, g.subtracted, sizeof(double)*sz);
        }
    } else {
        if (_config.normalize) {
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

    _plot->invalidateGraph();
    if (_config.normalize)
        _plot->setResult(r, 0, 1);
    else _plot->setResult(r, g.min, g.max);
    _table->setResult(r, loadTime, calcTime);
}
