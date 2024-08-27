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

enum CamDataRow { ROW_LOAD_TIME, ROW_CALC_TIME };

StillImageCamera::StillImageCamera(PlotIntf *plot, TableIntf *table) : Camera(plot, table, "StillImageCamera")
{
    loadConfig();

    Ori::Settings s;
    s.beginGroup(_configGroup);

    QString fileName = QFileDialog::getOpenFileName(qApp->activeWindow(),
                                qApp->tr("Open Beam Image"),
                                s.strValue("recentDir"),
                                qApp->tr("Images (*.png *.pgm *.jpg);;All Files (*.*)"));
    if (!fileName.isEmpty()) {
        _fileName = fileName;
        s.setValue("recentDir", QFileInfo(fileName).dir().absolutePath());
        s.setValue("recentFile", fileName);
    }
}

StillImageCamera::StillImageCamera(PlotIntf *plot, TableIntf *table, const QString& fileName) :
    Camera(plot, table, "StillImageCamera"), _fileName(fileName)
{
    loadConfig();

    Ori::Settings s;
    s.beginGroup(_configGroup);

    if (_fileName.isEmpty())
        _fileName = s.value("recentFile").toString();

    if (_fileName.isEmpty() || !QFile::exists(fileName))
        _fileName = qApp->applicationDirPath() + "/beam.png";

    s.setValue("recentFile", _fileName);
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

int StillImageCamera::bpp() const
{
    return _image.depth();
}

QList<QPair<int, QString>> StillImageCamera::dataRows() const
{
    return {
        { ROW_LOAD_TIME, qApp->tr("Load time") },
        { ROW_CALC_TIME, qApp->tr("Calc time") },
    };
}

void StillImageCamera::startCapture()
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
    c.bpp = fmt == QImage::Format_Grayscale16 ? 16 : 8;
    c.buf = (uint8_t*)buf;

    int sz = c.w*c.h;

    _plot->initGraph(c.w, c.h);
    double *graph = _plot->rawGraph();

    CgnBeamResult r;
    memset(&r, 0, sizeof(CgnBeamResult));

    CgnBeamBkgnd g;
    memset(&g, 0, sizeof(CgnBeamBkgnd));
    g.max_iter = _config.bgnd.iters;
    g.precision = _config.bgnd.precision;
    g.corner_fraction = _config.bgnd.corner;
    g.nT = _config.bgnd.noise;
    g.mask_diam = _config.bgnd.mask;
    if (_config.roi.on && _config.roi.isValid()) {
        g.ax1 = qRound(_config.roi.left * double(c.w));
        g.ay1 = qRound(_config.roi.top * double(c.h));
        g.ax2 = qRound(_config.roi.right * double(c.w));
        g.ay2 = qRound(_config.roi.bottom * double(c.h));
        r.x1 = g.ax1;
        r.y1 = g.ay1;
        r.x2 = g.ax2;
        r.y2 = g.ay2;
    } else {
        g.ax2 = c.w;
        g.ay2 = c.h;
        r.x2 = c.w;
        r.y2 = c.h;
    }
    QVector<double> subtracted;
    if (!_rawView && _config.bgnd.on) {
        subtracted = QVector<double>(sz);
        g.subtracted = subtracted.data();
    }

    timer.restart();
    if (!_rawView)
    {
        if (_config.bgnd.on) {
            cgn_calc_beam_bkgnd(&c, &g, &r);
        } else {
            cgn_calc_beam_naive(&c, &r);
        }
    }
    auto calcTime = timer.elapsed();

    timer.restart();
    const double rangeTop = (1 << c.bpp) - 1;

    if (_rawView)
    {
        cgn_copy_to_f64(&c, graph, &g.max);
        _plot->invalidateGraph();
        r.nan = true;
        _plot->setResult(r, 0, rangeTop);
        _table->setResult(r, {
            { ROW_LOAD_TIME, {loadTime} },
            { ROW_CALC_TIME, {calcTime} },
        });
        return;
    }

    if (_config.bgnd.on) {
        if (_config.plot.normalize) {
            cgn_copy_normalized_f64(g.subtracted, graph, sz, g.min,
                _config.plot.fullRange ? rangeTop-g.min : g.max);
        } else {
            memcpy(graph, g.subtracted, sizeof(double)*sz);
        }
    } else {
        if (_config.plot.normalize) {
            if (c.bpp > 8) {
                auto buf = (const uint16_t*)c.buf;
                cgn_render_beam_to_doubles_norm_16(buf, sz, graph,
                    _config.plot.fullRange ? rangeTop : cgn_find_max_16(buf, c.w*c.h));
            } else {
                cgn_render_beam_to_doubles_norm_8(c.buf, sz, graph,
                    _config.plot.fullRange ? rangeTop : cgn_find_max_8(c.buf, c.w*c.h));
            }
        } else {
            cgn_copy_to_f64(&c, graph, &g.max);
        }
    }
    auto copyTime = timer.elapsed();

    qDebug() << "loadTime:" << loadTime << "| calcTime" << calcTime << "| copyTime" << copyTime
        << "| iters" << g.iters << "| min" << g.min << "| max" << g.max
        << "| mean" << g.mean << "| sdev" << g.sdev << "| count" << g.count
        << "| ax1" << g.ax1 << "| ay1" << g.ay1 << "| ax2" << g.ax2 << "| ay2" << g.ay2
        << "| xx" << r.xx << "| yy" << r.yy << "| xy" << r.xy << "| p" << r.p;

    _plot->invalidateGraph();
    if (_config.plot.normalize)
        _plot->setResult(r, 0, 1);
    else {
        if (_config.plot.fullRange)
            _plot->setResult(r, 0, rangeTop);
        else
            _plot->setResult(r, g.min, g.max);
    }
    _table->setResult(r, {
        { ROW_LOAD_TIME, {loadTime} },
        { ROW_CALC_TIME, {calcTime} },
    });
}

void StillImageCamera::setRawView(bool on, bool reconfig)
{
    _rawView = on;
}
