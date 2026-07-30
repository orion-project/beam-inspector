#include "StillImageCamera.h"

#include "app/ImageUtils.h"
#ifdef WITH_IDS
#include "cameras/IdsCamera.h"
#endif
#include "widgets/PlotIntf.h"
#include "widgets/StabilityIntf.h"
#include "widgets/TableIntf.h"

#include "beam_calc.h"

#include "tools/OriSettings.h"
#include "helpers/OriDialogs.h"

#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QMessageBox>

#define LOG_ID "StillImageCamera:"

enum CamDataRow { ROW_LOAD_TIME, ROW_CALC_TIME };

StillImageCamera::StillImageCamera(PlotIntf *plot, TableIntf *table, StabilityIntf *stabil) : 
    Camera(plot, table, stabil, "StillImageCamera")
{
    loadConfig();

    Ori::Settings s;
    s.beginGroup(_configGroup);
    
    QString selectedFilter = s.strValue("recentFilter");

    QString fileName = QFileDialog::getOpenFileName(qApp->activeWindow(),
                                qApp->tr("Open Beam Image"),
                                s.strValue("recentDir"),
                                CameraCommons::supportedImageFilters(),
                                &selectedFilter);
    if (!fileName.isEmpty()) {
        _fileName = fileName;
        s.setValue("recentDir", QFileInfo(fileName).dir().absolutePath());
        s.setValue("recentFile", fileName);
        s.setValue("recentFilter", selectedFilter);
    }
}

StillImageCamera::StillImageCamera(PlotIntf *plot, TableIntf *table, StabilityIntf *stabil, const QString& fileName) :
    Camera(plot, table, stabil, "StillImageCamera"), _fileName(fileName)
{
    loadConfig();

    Ori::Settings s;
    s.beginGroup(_configGroup);

    if (_fileName.isEmpty())
        _fileName = s.value("recentFile").toString();

    if (_fileName.isEmpty() || !QFile::exists(_fileName)) {
        QString fallbackFile = qApp->applicationDirPath() + "/beam.png";
        if (!_fileName.isEmpty()) // File name is empty when the app starts normally
            qWarning() << "File does not exists:" << _fileName << "| Opting to demo:" << fallbackFile;
        _fileName = fallbackFile;
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
    return _width;
}

int StillImageCamera::height() const
{
    return _height;
}

int StillImageCamera::bpp() const
{
    return _bpp;
}

TableRowsSpec StillImageCamera::tableRows() const
{
    auto rows = Camera::tableRows();
    rows.aux
        << qMakePair(ROW_LOAD_TIME, qApp->tr("Load time"))
        << qMakePair(ROW_CALC_TIME, qApp->tr("Calc time"));
    return rows;
}

struct LoadResult
{
    QString error;
    QByteArray data;
    int width = 0;
    int height = 0;
    int bpp = 0;
};

static LoadResult loadRawFrame(const QString& fileName)
{
    QString infoFile = RawFrameData::infoFileName(fileName);
    if (!QFile::exists(infoFile))
        return { .error = qApp->tr("Info file not found for this raw frame data file") };
        
    QSettings info(infoFile, QSettings::IniFormat);
    int width = info.value(RawFrameData::keyWidth()).toInt();
    int height = info.value(RawFrameData::keyHeight()).toInt();
    const auto size = info.value(RawFrameData::keySize()).toInt();
    const auto pixelFormat = info.value(RawFrameData::keyPixelFormat()).toString();
    const auto camType = info.value(RawFrameData::keyCameraType()).toString();
    
    qDebug() << LOG_ID << fileName << "Raw" << camType << "frame" 
        << width << 'x' << height << "pixelFormat:" << pixelFormat;
    
    if (width <= 0 || height <= 0)
        return { .error = qApp->tr("Invalid image size %1 x %2").arg(width).arg(height) };

    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly))
        return { .error = qApp->tr("Unable to open raw file: %1").arg(f.errorString()) };

    QByteArray rawData = f.readAll();
    if (rawData.size() != size)
        return { .error = qApp->tr("Invalid read data size %1, expected %2").arg(rawData.size()).arg(size) };
        
    if (camType == RawFrameData::cameraTypeDemo())
    {
        return {
            .data = rawData,
            .width = width,
            .height = height,
            .bpp = 8,
        };
    }

 #ifdef WITH_IDS
    if (camType == RawFrameData::cameraTypeIds())
    {
        const auto& pf = IdsPixelFormat::findByName(pixelFormat);
        if (!pf.canDecode())
            return { .error = qApp->tr("Unsupported pixel format") };

        QByteArray data(width * height * pf.bytesPerPixel, 0);
        IdsPixelFormat::getDecodeFunc(pf)
            ((uint8_t*)data.data(), (uint8_t*)rawData.data(), rawData.size());

        return {
            .data = data,
            .width = width,
            .height = height,
            .bpp = pf.bitsPerPixel,
        };
    }
#endif
    
    return { .error = qApp->tr("Unsupported file type") };
}

static LoadResult loadPgmImage(const QString& fileName)
{
    // QImage does not support PGM images with more than 8-bit data (Qt 6.2, 6.9).
    // It can load them, but they are scaled down to 8-bit during loading.
    // So we load them manually.

    ImageUtils::PgmData pgm = ImageUtils::loadPgm(fileName);
    if (!pgm.error.isEmpty())
        return { .error = qApp->tr("Unable to load PGM file: %1").arg(pgm.error) };

    qDebug() << LOG_ID << fileName << "PGM" << "bits:" << pgm.bpp;

    return {
        .data = pgm.data,
        .width = pgm.width,
        .height = pgm.height,
        .bpp = pgm.bpp,
    };
}

static LoadResult loadAnyImage(const QString& fileName)
{
    QImage image(fileName);
    if (image.isNull())
        return { .error = qApp->tr("Unable to load image file") };

    qDebug() << LOG_ID << fileName << image.format() << "bits:" << image.depth();
    
    auto fmt = image.format();

    // TODO: Check for grayscale data layout
    // QImage can read grayscale jpegs if they saved by QImage
    // e.g. do "Export raw image", save as jpg, then open - it should be displayed properly.
    // But it shows something strange for jpg saved by Photoshop (see beams/green_pointer_gray.jpg)
    // even though visually this image looks ok (check it with exportImageDlg(image)).
    // It seems there can be different layouts for grayscale images
    // and sometimes we can not just use image.bits() directly 
    if (fmt != QImage::Format_Grayscale8 && fmt != QImage::Format_Grayscale16)
    {
        return { .error = qApp->tr("Wrong image format, only grayscale images are supported") };

        // TODO: convert colored images to grayscale
        // It's somehow related to the above issue with different grayscale data layouts
        // image.convertTo() produces a layout that can not be used directly via image.bits()
        /*
        if (image.depth() > 8)
        {
            // TODO: image.depth() is 32 for colored RGBA images
            // but it should be converted to 8-bit grayscale
            fmt = QImage::Format_Grayscale16;
        }
        else
            fmt = QImage::Format_Grayscale8;

        image.convertTo(fmt, Qt::ColorOnly);
        if (image.isNull())
            return { .error = qApp->tr("Unsupported image format") };
            
        qDebug() << LOG_ID << fileName << "converted to" << image.format() << "bits:" << image.depth();
        */
    }

    // display image for visual check
    // #include "plot/PlotExport.h"
    // exportImageDlg(image); 
    
    QByteArray data((const char*)image.constBits(), image.sizeInBytes());

    return {
        .data = data,
        .width = image.width(),
        .height = image.height(),
        .bpp = image.depth(),
    };
}

static LoadResult loadImage(const QString& fileName)
{
    if (RawFrameData::isRawFileName(fileName))
        return loadRawFrame(fileName);
        
    if (fileName.endsWith(".pgm", Qt::CaseInsensitive))
        return loadPgmImage(fileName);
    
    return loadAnyImage(fileName);
}

void StillImageCamera::startCapture()
{
    QElapsedTimer timer;
    timer.start();

    auto res = loadImage(_fileName);
    if (!res.error.isEmpty())
        Ori::Dlg::Defer::error(res.error);

    auto loadTime = timer.elapsed();
    qDebug() << LOG_ID << "Loaded in" << loadTime << "ms";

    CgnBeamCalc c;
    c.w = _width = res.width;
    c.h = _height = res.height;
    c.bpp = _bpp = res.bpp;
    c.buf = (uint8_t*)res.data.data();

    _plot->initGraph(c.w, c.h);
    double *graph = _plot->rawGraph();

    CgnBeamResult r;
    memset(&r, 0, sizeof(CgnBeamResult));
    QList<CgnBeamResult> results;

    if (_rawView)
    {
        cgn_copy_to_f64(&c, graph, nullptr);
        _plot->invalidateGraph();
        _plot->setResult({}, 0, (1 << c.bpp) - 1);
        _table->setResult({}, {}, {
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

    auto roiMode = _config.roiMode;

    auto setRoi = [&c, &g, &r, roiMode](const RoiRect &roi){
        if (roiMode != ROI_NONE && roi.isValid()) {
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
    if (_config.roiMode == ROI_MULTI)
    {
        if (subtract) {
            g.min = 1e10;
            g.max = -1e10;
            g.subtract_bkgnd_v = 1;
            cgn_copy_to_f64(&c, g.subtracted, nullptr);
        }
        for (const auto &roi : std::as_const(_config.rois)) {
            setRoi(roi);
            cgn_calc_beam_bkgnd(&c, &g, &r);
            results << r;
        }
    }
    else
    {
        setRoi(_config.roi);
        cgn_calc_beam_bkgnd(&c, &g, &r);
        results << r;
    }
    auto calcTime = timer.elapsed();

    double minZ, maxZ;
    cgn_ext_copy_to_f64(&c, &g, graph, _config.plot.normalize, _config.plot.fullRange, &minZ, &maxZ);
    _plot->invalidateGraph();
    _plot->setResult(results, minZ, maxZ);

    _table->setResult(results, {}, {
        { ROW_LOAD_TIME, {loadTime} },
        { ROW_CALC_TIME, {calcTime} },
    });
    
    _stabil->setResult(0, {});
}

void StillImageCamera::setRawView(bool on, bool reconfig)
{
    _rawView = on;
}
