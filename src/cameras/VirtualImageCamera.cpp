#include "VirtualImageCamera.h"

#include "cameras/CameraWorker.h"

#include "dialogs/OriConfigDlg.h"
#include "helpers/OriDialogs.h"

#include <QPainter>
#include <QSettings>

#define LOG_ID "VirtualImageCamera:"
#define CAMERA_LOOP_TICK_MS 5
#define CAMERA_FRAME_DELAY_MS 30
#define CAMERA_HARD_FPS 30
//#define LOG_FRAME_TIME

enum CamDataRow { ROW_RENDER_TIME, ROW_CALC_TIME, ROW_POWER };

class ImageCameraWorker : public CameraWorker
{
public:
    VirtualImageCamera *cam;
    QImage image;

    QImage jitterImg;
    RandomOffset jitterX;
    RandomOffset jitterY;
    RandomOffset jitterA;
    double centerX, centerY;

    ImageCameraWorker(PlotIntf *plot, TableIntf *table, VirtualImageCamera *cam, QThread *thread)
        : CameraWorker(plot, table, cam, cam, LOG_ID), cam(cam)
    {
        tableData = [this]{
            QMap<int, CamTableData> data = {
                { ROW_RENDER_TIME, {avgAcqTime} },
                { ROW_CALC_TIME, {avgCalcTime} },
            };
            if (showPower)
                data[ROW_POWER] = {
                    QVariantList{
                        power * powerScale,
                        powerSdev * powerScale,
                        powerDecimalFactor
                    },
                    CamTableData::POWER,
                    hasPowerWarning
                };
            return data;
        };
    }

    QString init()
    {
        image = QImage(cam->_fileName);
        if (image.isNull()) {
           return qApp->tr("Unable to to load image file");
        }
        auto fmt = image.format();
        if (fmt != QImage::Format_Grayscale8 && fmt != QImage::Format_Grayscale16) {
            return qApp->tr("Wrong image format, only grayscale images are supported");
        }

        c.w = image.width();
        c.h = image.height();
        c.bpp = fmt == QImage::Format_Grayscale16 ? 16 : 8;

        jitterX = RandomOffset(-cam->_jitterShift, cam->_jitterShift);
        jitterY = RandomOffset(-cam->_jitterShift, cam->_jitterShift);
        jitterA = RandomOffset(-cam->_jitterAngle, cam->_jitterAngle);
        jitterImg = QImage(image.size(), image.format());
        jitterImg.fill(0);

        centerX = cam->_centerX;
        centerY = cam->_centerY;
        if (centerX < 0 or centerX > c.w) centerX = c.w/2.0;
        if (centerY < 0 or centerY > c.h) centerY = c.h/2.0;

        plot->initGraph(c.w, c.h);
        graph = plot->rawGraph();

        configure();
        togglePowerMeter();

        return {};
    }

    inline bool waitFrame()
    {
        tm = timer.elapsed();
        if (tm - prevFrame < CAMERA_FRAME_DELAY_MS) {
            // Sleep gives a bad precision because OS decides how long the thread should sleep.
            // When we disable sleep, its possible to get an exact number of FPS,
            // e.g. 40 FPS when CAMERA_FRAME_DELAY_MS=25, but at cost of increased CPU usage.
            // Sleep allows to get about 30 FPS, which is enough, and relaxes CPU loading
            thread->msleep(CAMERA_LOOP_TICK_MS);
            return true;
        }
        avgFrameCount++;
        avgFrameTime += tm - prevFrame;
        prevFrame = tm;
        return false;
    }

    void makeJitterImg() {
        QPainter p(&jitterImg);
        p.translate(centerX, centerY);
        p.rotate(jitterA.value());
        p.drawImage(QPoint(-centerX + jitterX.value(), -centerY + jitterY.value()), image);

        jitterX.next();
        jitterY.next();
        jitterA.next();

        // declare explicitly as const to avoid deep copy
        const uchar* buf = jitterImg.bits();
        c.buf = (uint8_t*)buf;
    }

    void run() {
        startCapture();
        while (true) {
            if (waitFrame()) continue;

            tm = timer.elapsed();
            makeJitterImg();
            markAcqTime();

            tm = timer.elapsed();
            calcResult();
            markCalcTime();

            if (showResults())
                emit cam->ready();

            if (tm - prevStat >= STAT_DELAY_MS) {
                prevStat = tm;

                double ft = avgFrameTime / avgFrameCount;
                avgFrameTime = 0;
                avgFrameCount = 0;
                CameraStats st {
                    .fps = 1000.0/ft,
                    .hardFps = CAMERA_HARD_FPS,
                    .measureTime = measureStart > 0 ? timer.elapsed() - measureStart : -1,
                };
                emit cam->stats(st);
#ifdef LOG_FRAME_TIME
                qDebug()
                    << "FPS:" << st.fps
                    << "avgFrameTime:" << qRound(ft)
                    << "avgRenderTime:" << qRound(avgAcqTime)
                    << "avgCalcTime:" << qRound(avgCalcTime);
#endif
                if (cam->isInterruptionRequested()) {
                    qDebug() << LOG_ID << "Interrupted by user";
                    return;
                }
                checkReconfig();
            }
        }
    }
};

VirtualImageCamera::VirtualImageCamera(PlotIntf *plot, TableIntf *table, QObject *parent) :
    Camera(plot, table, "VirtualImageCamera"), QThread(parent)
{
    loadConfig();

    auto render = new ImageCameraWorker(plot, table, this, this);
    auto res = render->init();
    if (!res.isEmpty())
    {
        Ori::Dlg::error(res);
        delete render;
        return;
    }
    _render.reset(render);

    connect(parent, SIGNAL(camConfigChanged()), this, SLOT(camConfigChanged()));
}

int VirtualImageCamera::width() const
{
    return _render ? _render->image.width() : 0;
}

int VirtualImageCamera::height() const
{
    return _render ? _render->image.height() : 0;
}

int VirtualImageCamera::bpp() const
{
    return _render ? _render->image.depth() : 0;
}

TableRowsSpec VirtualImageCamera::tableRows() const
{
    auto rows = Camera::tableRows();
    rows.aux
        << qMakePair(ROW_RENDER_TIME, qApp->tr("Render time"))
        << qMakePair(ROW_CALC_TIME, qApp->tr("Calc time"));
    if (_config.power.on)
        rows.aux << qMakePair(ROW_POWER, qApp->tr("Power"));
    return rows;
}

QList<QPair<int, QString>> VirtualImageCamera::measurCols() const
{
    QList<QPair<int, QString>> cols;
    if (_config.power.on)
        cols << qMakePair(COL_POWER, qApp->tr("Power"));
    return cols;
}

void VirtualImageCamera::startCapture()
{
    start();
}

void VirtualImageCamera::startMeasure(MeasureSaver *saver)
{
    if (_render)
        _render->startMeasure(saver);
}

void VirtualImageCamera::stopMeasure()
{
    if (_render)
        _render->stopMeasure();
}

void VirtualImageCamera::run()
{
    if (_render)
        _render->run();
}

void VirtualImageCamera::camConfigChanged()
{
    if (_render)
        _render->reconfigure();
}

void VirtualImageCamera::requestRawImg(QObject *sender)
{
    if (_render)
        _render->requestRawImg(sender);
}

void VirtualImageCamera::setRawView(bool on, bool reconfig)
{
    if (_render)
        _render->setRawView(on, reconfig);
}

void VirtualImageCamera::togglePowerMeter()
{
    if (_render)
        _render->togglePowerMeter();
}

void VirtualImageCamera::raisePowerWarning()
{
    if (_render)
        _render->hasPowerWarning = true;
}

void VirtualImageCamera::saveConfigMore(QSettings *s)
{
    s->setValue("sourceImg", _fileName);
    s->setValue("jitter.center.x", _centerX);
    s->setValue("jitter.center.y", _centerY);
    s->setValue("jitter.angle", _jitterAngle);
    s->setValue("jitter.shift", _jitterShift);
}

void VirtualImageCamera::loadConfigMore(QSettings *s)
{
    _fileName = s->value("sourceImg").toString();
    if (_fileName.isEmpty())
        _fileName = qApp->applicationDirPath() + "/beam.png";
    _centerX = s->value("jitter.center.x", -1).toInt();
    _centerY = s->value("jitter.center.y", -1).toInt();
    _jitterAngle = s->value("jitter.angle", 15).toInt();
    _jitterShift = s->value("jitter.shift", 15).toInt();
}

void VirtualImageCamera::initConfigMore(Ori::Dlg::ConfigDlgOpts &opts)
{
    int pageImg = cfgMax + 1;
    opts.pages << Ori::Dlg::ConfigPage(pageImg, tr("Image"), ":/toolbar/raw_view");
    opts.items
        << new Ori::Dlg::ConfigItemEmpty(pageImg, tr("Reselect camera to apply paramaters"))
        << (new Ori::Dlg::ConfigItemFile(pageImg, tr("Source image"), &_fileName))
            ->withFilter(CameraCommons::supportedImageFilters())
        << (new Ori::Dlg::ConfigItemInt(pageImg, tr("Position jitter (px)"), &_jitterShift))
               ->withMinMax(-500, 500)
        << (new Ori::Dlg::ConfigItemInt(pageImg, tr("Angle jitter (deg)"), &_jitterAngle))
           ->withMinMax(-15, 15)
        << (new Ori::Dlg::ConfigItemInt(pageImg, tr("Rotation center X (px)"), &_centerX))
               ->withMinMax(-1, 10000)
               ->withHint(tr("Set to -1 to use image center"))
        << (new Ori::Dlg::ConfigItemInt(pageImg, tr("Rotation center Y (px)"), &_centerY))
               ->withMinMax(-1, 10000)
               ->withHint(tr("Set to -1 to use image center"))
    ;
}
