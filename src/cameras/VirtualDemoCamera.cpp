#include "VirtualDemoCamera.h"

#include "cameras/CameraWorker.h"

#include "beam_render.h"

#define LOG_ID "VirtualDemoCamera:"
#define CAMERA_WIDTH 2592
#define CAMERA_HEIGHT 2048
#define CAMERA_LOOP_TICK_MS 5
#define CAMERA_FRAME_DELAY_MS 30
#define CAMERA_HARD_FPS 30
//#define LOG_FRAME_TIME

enum CamDataRow { ROW_RENDER_TIME, ROW_CALC_TIME, ROW_POWER };

class BeamRenderer : public CameraWorker
{
public:
    VirtualDemoCamera *cam;

    CgnBeamRender b;
    QVector<uint8_t> d;

    RandomOffset dx_offset;
    RandomOffset dy_offset;
    RandomOffset xc_offset;
    RandomOffset yc_offset;
    RandomOffset phi_offset;

    BeamRenderer(PlotIntf *plot, TableIntf *table, StabilityIntf *stabil, VirtualDemoCamera *cam, QThread *thread)
        : CameraWorker(plot, table, stabil, cam, cam, LOG_ID), cam(cam)
    {
        b.w = CAMERA_WIDTH;
        b.h = CAMERA_HEIGHT;
        b.dx = b.w/2;
        b.dy = b.dx*0.75;
        b.xc = b.w/2;
        b.yc = b.h/2;
        b.p = 255;
        b.phi = 12;
        d = QVector<uint8_t>(b.w * b.h);
        b.buf = d.data();

        c.w = b.w;
        c.h = b.h;
        c.buf = b.buf;
        c.bpp = 8;

        dx_offset = RandomOffset(b.dx, b.dx-20, b.dx+20);
        dy_offset = RandomOffset(b.dy, b.dy-20, b.dy+20);
        xc_offset = RandomOffset(b.xc, b.xc-20, b.xc+20);
        yc_offset = RandomOffset(b.yc, b.yc-20, b.yc+20);
        phi_offset = RandomOffset(b.phi, b.phi-12, b.phi+12);

        plot->initGraph(c.w, c.h);
        graph = plot->rawGraph();

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
        
        rawFrameData = [this]{
            return RawFrameData {
                .data = QByteArray((const char*)c.buf, c.w*c.h),
                .pixelFormat = QImage::Format_Grayscale8,
                .cameraType = RawFrameData::cameraTypeDemo(),
                .cameraModel = this->cam->name(),
            };
        };

        configure();
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

    void run() {
        startCapture();
        while (true) {
            if (waitFrame()) continue;

            tm = timer.elapsed();
            cgn_render_beam_tilted(&b);
            markAcqTime();

            b.dx = dx_offset.next();
            b.dy = dy_offset.next();
            b.xc = xc_offset.next();
            b.yc = yc_offset.next();
            b.phi = phi_offset.next();

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

VirtualDemoCamera::VirtualDemoCamera(PlotIntf *plot, TableIntf *table, StabilityIntf *stabil, QObject *parent) :
    Camera(plot, table, stabil, "VirtualDemoCamera"), QThread(parent)
{
    loadConfig();

    _render.reset(new BeamRenderer(plot, table, stabil, this, this));
    _render->togglePowerMeter();

    connect(parent, SIGNAL(camConfigChanged()), this, SLOT(camConfigChanged()));
}

int VirtualDemoCamera::width() const
{
    return CAMERA_WIDTH;
}

int VirtualDemoCamera::height() const
{
    return CAMERA_HEIGHT;
}

TableRowsSpec VirtualDemoCamera::tableRows() const
{
    auto rows = Camera::tableRows();
    rows.aux
        << qMakePair(ROW_RENDER_TIME, qApp->tr("Render time"))
        << qMakePair(ROW_CALC_TIME, qApp->tr("Calc time"));
    if (_config.power.on)
        rows.aux << qMakePair(ROW_POWER, qApp->tr("Power"));
    return rows;
}

QList<QPair<int, QString>> VirtualDemoCamera::measurCols() const
{
    QList<QPair<int, QString>> cols;
    if (_config.power.on)
        cols << qMakePair(COL_POWER, qApp->tr("Power"));
    return cols;
}

void VirtualDemoCamera::startCapture()
{
    start();
}

void VirtualDemoCamera::startMeasure(MeasureSaver *saver)
{
    _render->startMeasure(saver);
}

void VirtualDemoCamera::stopMeasure()
{
    _render->stopMeasure();
}

void VirtualDemoCamera::run()
{
    _render->run();
}

void VirtualDemoCamera::camConfigChanged()
{
    _render->reconfigure();
}

void VirtualDemoCamera::requestRawImg(QObject *sender)
{
    _render->requestRawImg(sender);
}

void VirtualDemoCamera::setRawView(bool on, bool reconfig)
{
    _render->setRawView(on, reconfig);
}

void VirtualDemoCamera::togglePowerMeter()
{
    _render->togglePowerMeter();
}

void VirtualDemoCamera::raisePowerWarning()
{
    _render->hasPowerWarning = true;
}
