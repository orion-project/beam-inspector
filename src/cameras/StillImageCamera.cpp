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

    QString recentFilter = s.strValue("recentFilter");
    
    QString fileName = QFileDialog::getOpenFileName(qApp->activeWindow(),
                                qApp->tr("Open Beam Image"),
                                s.strValue("recentDir"),
                                CameraCommons::supportedImageFilters(),
                                &recentFilter);
    if (!fileName.isEmpty()) {
        _fileName = fileName;
        s.setValue("recentDir", QFileInfo(fileName).dir().absolutePath());
        s.setValue("recentFile", fileName);
        s.setValue("recentFilter", recentFilter);
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

void StillImageCamera::startCapture()
{
    QElapsedTimer timer;
    timer.start();
    
    CgnBeamCalc c;
    QImage image;
    QByteArray pgmData;
    QByteArray rawData;
    qint64 loadTime;
    
    if (RawFrameData::isRawFileName(_fileName)) {
        QString infoFile = RawFrameData::infoFileName(_fileName);
        if (!QFile::exists(infoFile)) {
            Ori::Dlg::error(qApp->tr("Info file not found for this raw frame data file"));
            return;
        }
        QSettings info(infoFile, QSettings::IniFormat);
        _width = info.value(RawFrameData::keyWidth()).toInt();
        _height = info.value(RawFrameData::keyHeight()).toInt();
        const auto size = info.value(RawFrameData::keySize()).toInt();
        const auto pixelFormat = info.value(RawFrameData::keyPixelFormat()).toString();
        const auto camType = info.value(RawFrameData::keyCameraType()).toString();
        qDebug() << LOG_ID << _fileName << "Raw" << camType << "frame" << _width << 'x' << _height << "pixelFormat:" << pixelFormat;
        if (_width <= 0 || _height <= 0) {
            Ori::Dlg::error(qApp->tr("Invalid image size %1 x %2").arg(_width).arg(_height));
            return;
        }
        QFile f(_fileName);
        if (!f.open(QIODevice::ReadOnly)) {
            Ori::Dlg::error(qApp->tr("Unable to open raw file: %1").arg(f.errorString()));
            return;
        }
        rawData = f.readAll();
        if (rawData.size() != size) {
            Ori::Dlg::error(qApp->tr("Invalid read data size %1, expected %2").arg(rawData.size()).arg(size));
            return;
        }
        if (camType == RawFrameData::cameraTypeDemo()) {
            _bpp = 8;
        }
    #ifdef WITH_IDS
        else if (camType == RawFrameData::cameraTypeIds()) {
            int fmt = 0;
            auto fmts = IdsCamera::pixelFormats();
            for (const auto &f : std::as_const(fmts))
                if (f.name == pixelFormat) {
                    fmt = f.code;
                    break;
                }
            if (fmt == IdsCamera::supportedPixelFormat_Mono8()) {
                _bpp = 8;
            } else if (fmt == IdsCamera::supportedPixelFormat_Mono10G40()) {
                _bpp = 10;
                pgmData = QByteArray(_width * _height * sizeof(uint16_t), 0);
                cgn_convert_10g40_to_u16((uint8_t*)pgmData.data(), (uint8_t*)rawData.data(), rawData.size());
            } else if (fmt == IdsCamera::supportedPixelFormat_Mono12G24()) {
                _bpp = 12;
                pgmData = QByteArray(_width * _height * sizeof(uint16_t), 0);
                cgn_convert_12g24_to_u16((uint8_t*)pgmData.data(), (uint8_t*)rawData.data(), rawData.size());
            } else {
                Ori::Dlg::error(qApp->tr("Unsupported pixel format"));
                return;
            }
        }
     #endif
        else {
            Ori::Dlg::error(qApp->tr("Unsupported file type"));
            return;
        }
        c.w = _width;
        c.h = _height;
        c.bpp = _bpp;
        c.buf = (uint8_t*)(pgmData.size() > 0 ? pgmData.data() : rawData.data());
    }
    // QImage does not support PGM images with more than 8-bit data (Qt 6.2, 6.9).
    // It can load them, but they are scaled down to 8-bit during loading.
    else if (_fileName.endsWith(".pgm", Qt::CaseInsensitive)) {
        ImageUtils::PgmData pgm = ImageUtils::loadPgm(_fileName);
        if (!pgm.error.isEmpty()) {
            Ori::Dlg::error(qApp->tr("Unable to load PGM file: %1").arg(pgm.error));
            return;
        }
        loadTime = timer.elapsed();
        qDebug() << LOG_ID << _fileName << "PGM" << "bits:" << pgm.bpp << "loaded in" << loadTime << "ms";

        pgmData = pgm.data;
        _width = pgm.width;
        _height = pgm.height;
        _bpp = pgm.bpp;
        
        c.w = pgm.width;
        c.h = pgm.height;
        c.bpp = pgm.bpp;
        c.buf = (uint8_t*)pgmData.data();
    } else {
        image = QImage(_fileName);
        if (image.isNull()) {
            Ori::Dlg::error(qApp->tr("Unable to load image file"));
            return;
        }
        loadTime = timer.elapsed();
        qDebug() << LOG_ID << _fileName << image.format() << "bits:" << image.depth() << "loaded in" << loadTime << "ms";
        
        auto fmt = image.format();
        // TODO: Check for grayscale data layout
        // QImage can read grayscale jpegs if they saved by QImage
        // e.g. do "Export raw image", save as jpg, then open - it should be displayed properly.
        // But it shows something strange for the gryascale jpg saved by Photoshop (see beams/green_pointer_gray.jpg)
        // Though visually this image looks ok (check it with exportImageDlg(image))
        // It seems there are different layouts for grayscale images
        // and sometimes we can just use image.bits() directly 
        if (fmt != QImage::Format_Grayscale8 && fmt != QImage::Format_Grayscale16) {
            Ori::Dlg::error(qApp->tr("Wrong image format, only grayscale images are supported"));
            return;
        
            // TODO: convert colored images to grayscale
            // It's somehow related to the above TODO about different grayscale dat alayouts
            // image.convertTo() produces a layout than can be used directly via image.bits()
            // TODO: image.depth() is 32 for colored RGBA images but it should be converted to 8-bit gray
            if (image.depth() > 8) {
                fmt = QImage::Format_Grayscale16;
            } else {
                fmt = QImage::Format_Grayscale8;
            }
            image.convertTo(fmt, Qt::ColorOnly);
            if (image.isNull()) {
                Ori::Dlg::error(qApp->tr("Unsupported image format"));
                return;
            } else {
                qDebug() << LOG_ID << _fileName << "converted to" << image.format() << "bits:" << image.depth();
            }
        }
        //exportImageDlg(image); // display image for visual check
        
        // declare explicitly as const to avoid deep copy
        const uchar* buf = image.bits();
        
        _width = image.width();
        _height = image.height();
        _bpp = image.depth();
        
        c.w = image.width();
        c.h = image.height();
        c.bpp = fmt == QImage::Format_Grayscale16 ? 16 : 8;
        c.buf = (uint8_t*)buf;
    }

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
