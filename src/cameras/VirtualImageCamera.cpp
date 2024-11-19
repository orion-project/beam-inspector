#include "VirtualImageCamera.h"

#include "cameras/CameraWorker.h"

#include "helpers/OriDialogs.h"

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
    QString fileName;
    QImage image;

    QImage jitter_img;
    int jitter_xc = 0;
    int jitter_yc = 0;
    double jitter_phi = 0;
    RandomOffset xc_offset;
    RandomOffset yc_offset;
    RandomOffset phi_offset;

    ImageCameraWorker(PlotIntf *plot, TableIntf *table, VirtualImageCamera *cam, QThread *thread)
        : CameraWorker(plot, table, cam, cam, LOG_ID), cam(cam)
    {
        fileName = qApp->applicationDirPath() + "/beam.png";

        tableData = [this]{
            QMap<int, CamTableData> data = {
                { ROW_RENDER_TIME, {avgAcqTime} },
                { ROW_CALC_TIME, {avgCalcTime} },
            };
            if (showPower)
                data[ROW_POWER] = {QVariantList{r.p * powerScale, powerDecimalFactor}, CamTableData::POWER, hasPowerWarning};
            return data;
        };
    }

    QString init()
    {
        image = QImage(fileName);
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

        xc_offset = RandomOffset(jitter_xc, jitter_xc-20, jitter_xc+20);
        yc_offset = RandomOffset(jitter_yc, jitter_yc-20, jitter_yc+20);
        phi_offset = RandomOffset(jitter_phi, jitter_phi-12, jitter_phi+12);

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
        QTransform m;
        m.translate(c.w/2.0, c.h/2.0);
        m.rotate(jitter_phi);
        jitter_img = image.transformed(m);
        jitter_img = jitter_img.copy(
            jitter_img.width()/2 - c.w/2 + jitter_xc,
            jitter_img.height()/2 - c.h/2 + jitter_yc,
            c.w, c.h);
        jitter_img = jitter_img.convertToFormat(image.format());
        // qDebug() << "Jitter:" << image.width() << image.height() << image.format() << image.sizeInBytes() << "->"
        //          << jitter_img.width() << jitter_img.height() << jitter_img.format() << jitter_img.sizeInBytes();

        jitter_xc = xc_offset.next();
        jitter_yc = yc_offset.next();
        jitter_phi = phi_offset.next();

        // declare explicitly as const to avoid deep copy
        const uchar* buf = jitter_img.bits();
        c.buf = (uint8_t*)buf;
    }

    void run() {
        qDebug() << LOG_ID << "Started" << QThread::currentThreadId();
        start = QDateTime::currentDateTime();
        timer.start();
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

QList<QPair<int, QString> > VirtualImageCamera::dataRows() const
{
    QList<QPair<int, QString>> rows = {
        { ROW_RENDER_TIME, qApp->tr("Render time") },
        { ROW_CALC_TIME, qApp->tr("Calc time") },
    };
    if (_config.power.on)
        rows << qMakePair(ROW_POWER, qApp->tr("Power"));
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
