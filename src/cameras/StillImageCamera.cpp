#include "StillImageCamera.h"

#include "widgets/PlotIntf.h"
#include "widgets/TableIntf.h"

#include "beam_calc.h"

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
                                CameraCommons::supportedImageFilters());
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

    if (_fileName.isEmpty() || !QFile::exists(_fileName)) {
        _fileName = qApp->applicationDirPath() + "/beam.png";
        _demoMode = true;
    }

    if (!_demoMode)
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

TableRowsSpec StillImageCamera::tableRows() const
{
    auto rows = Camera::tableRows();
    if (_config.roiMode == ROI_NONE || _config.roiMode == ROI_SINGLE) {
        rows.results << qApp->tr("Centroid");
    } else {
        for (int i = 0; i < _config.rois.size(); i++) {
            if (_config.rois.at(i).on)
                rows.results << qApp->tr("Result #%1").arg(i+1);
        }
    }
    rows.aux
        << qMakePair(ROW_LOAD_TIME, qApp->tr("Load time"))
        << qMakePair(ROW_CALC_TIME, qApp->tr("Calc time"));
    return rows;
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

    _plot->initGraph(c.w, c.h);
    double *graph = _plot->rawGraph();

    CgnBeamResult r;
    memset(&r, 0, sizeof(CgnBeamResult));
    QList<CgnBeamResult> results;

    if (_rawView)
    {
        cgn_copy_to_f64(&c, graph, nullptr);
        _plot->invalidateGraph();
        r.nan = true;
        _plot->setResult(r, 0, (1 << c.bpp) - 1);
        results << r;
        _table->setResult(results, {
            { ROW_LOAD_TIME, {loadTime} },
            { ROW_CALC_TIME, {0} },
        });
        return;
    }

    CgnBeamBkgnd g;
    memset(&g, 0, sizeof(CgnBeamBkgnd));
    g.max_iter = _config.bgnd.iters;
    g.precision = _config.bgnd.precision;
    g.corner_fraction = _config.bgnd.corner;
    g.nT = _config.bgnd.noise;
    g.mask_diam = _config.bgnd.mask;

    auto setRoi = [&c, &g, &r](const RoiRect &roi){
        if (roi.on && roi.isValid()) {
            g.ax1 = qRound(roi.left * double(c.w));
            g.ay1 = qRound(roi.top * double(c.h));
            g.ax2 = qRound(roi.right * double(c.w));
            g.ay2 = qRound(roi.bottom * double(c.h));
        } else {
            g.ax1 = 0;
            g.ay1 = 0;
            g.ax2 = c.w;
            g.ay2 = c.h;
        }
        r.x1 = g.ax1;
        r.y1 = g.ay1;
        r.x2 = g.ax2;
        r.y2 = g.ay2;
    };

    bool subtract = _config.bgnd.on;
    QVector<double> subtracted;
    if (subtract) {
        subtracted = QVector<double>(c.w*c.h);
        g.subtracted = subtracted.data();
    }

    timer.restart();
    if (_config.roiMode == ROI_NONE || _config.roiMode == ROI_SINGLE)
    {
        setRoi(_config.roi);
        cgn_calc_beam_bkgnd(&c, &g, &r);
        results << r;
    }
    else
    {
        if (subtract) {
            g.min = 1e10;
            g.max = -1e10;
            g.subtract_bkgnd_v = 1;
            cgn_copy_to_f64(&c, g.subtracted, nullptr);
        }
        for (const auto &roi : _config.rois) {
            setRoi(roi);
            cgn_calc_beam_bkgnd(&c, &g, &r);
            results << r;
        }
    }
    auto calcTime = timer.elapsed();

    double minZ, maxZ;
    cgn_ext_copy_to_f64(&c, &g, graph, _config.plot.normalize, _config.plot.fullRange, &minZ, &maxZ);
    _plot->invalidateGraph();
    _plot->setResult(r, minZ, maxZ);

    _table->setResult(results, {
        { ROW_LOAD_TIME, {loadTime} },
        { ROW_CALC_TIME, {calcTime} },
    });
}

void StillImageCamera::setRawView(bool on, bool reconfig)
{
    _rawView = on;
}
