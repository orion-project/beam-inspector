#include "VirtualDemoCamera.h"

#include "widgets/PlotIntf.h"
#include "widgets/TableIntf.h"

#include "beam_calc.h"
#include "beam_render.h"

#include <QApplication>
#include <QDebug>
#include <QMutex>
#include <QRandomGenerator>

#define CAMERA_WIDTH 2592
#define CAMERA_HEIGHT 2048
#define CAMERA_LOOP_TICK_MS 5
#define CAMERA_FRAME_DELAY_MS 30
#define PLOT_FRAME_DELAY_MS 200
#define STAT_DELAY_MS 1000
#define MEASURE_BUF_SIZE 1000
#define MEASURE_BUF_COUNT 2
//#define LOG_FRAME_TIME

struct RandomOffset
{
    inline double rnd() {
        return QRandomGenerator::global()->generate() / rnd_max;
    }

    RandomOffset() {}

    RandomOffset(double start, double min, double max) : v(start), v_min(min), v_max(max) {
        dv = v_max - v_min;
        h = dv / 4.0;
        rnd_max = double(std::numeric_limits<quint32>::max());
    }

    double next() {
        v = qAbs(v + rnd()*h - h*0.5);
        if (v > dv)
            v = dv - rnd()*h;
        return v + v_min;
    }

    double v, dv, v_min, v_max, h, rnd_max;
};

class BeamRenderer
{
public:
    CgnBeamRender b;
    CgnBeamCalc c;
    CgnBeamResult r;
    CgnBeamBkgnd g;
    //CgnBeamCalcBlas c;
    //CgnBeamResultBlas r;
    QVector<uint8_t> d;
    RandomOffset dx_offset;
    RandomOffset dy_offset;
    RandomOffset xc_offset;
    RandomOffset yc_offset;
    RandomOffset phi_offset;
    qint64 prevFrame = 0;
    qint64 prevReady = 0;
    qint64 prevStat = 0;
    PlotIntf *plot;
    TableIntf *table;
    VirtualDemoCamera *cam;
    double avgFrameTime = 0;
    double avgRenderTime = 0;
    double avgCalcTime = 0;
    QVector<double> subtracted;
    bool subtract;
    bool normalize;
    double *graph;
    bool reconfig = false;
    QObject *saver;
    QMutex saverMutex;
    QVector<Measurement> resultBuf1;
    QVector<Measurement> resultBuf2;
    Measurement *resultBufs[MEASURE_BUF_COUNT];
    Measurement *results;
    int resultIdx = 0;
    int resultBufIdx = 0;
    QDateTime start;
    QElapsedTimer timer;
    qint64 measureStart = -1;

    BeamRenderer(PlotIntf *plot, TableIntf *table, VirtualDemoCamera *cam) : plot(plot), table(table), cam(cam)
    {
        plot->initGraph(CAMERA_WIDTH, CAMERA_HEIGHT);
        graph = plot->rawGraph();

        resultBuf1.resize(MEASURE_BUF_SIZE);
        resultBuf2.resize(MEASURE_BUF_SIZE);
        resultBufs[0] = resultBuf1.data();
        resultBufs[1] = resultBuf2.data();
        results = resultBufs[0];

        configure();
    }

    void configure() {
        auto cfg = cam->config();

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
        c.bits = 8;
//        if (cgn_calc_beam_blas_init(&c)) {
//            cgn_calc_beam_blas_free(&c);
//            qCritical() << "Failed to initialize calc buffers";
//        }

        dx_offset = RandomOffset(b.dx, b.dx-20, b.dx+20);
        dy_offset = RandomOffset(b.dy, b.dy-20, b.dy+20);
        xc_offset = RandomOffset(b.xc, b.xc-20, b.xc+20);
        yc_offset = RandomOffset(b.yc, b.yc-20, b.yc+20);
        phi_offset = RandomOffset(b.phi, b.phi-12, b.phi+12);

        memset(&r, 0, sizeof(CgnBeamResult));
        memset(&g, 0, sizeof(CgnBeamBkgnd));

        g.max_iter = cfg.bgnd.iters;
        g.precision = cfg.bgnd.precision;
        g.corner_fraction = cfg.bgnd.corner;
        g.nT = cfg.bgnd.noise;
        g.mask_diam = cfg.bgnd.mask;
        if (cfg.roi.on && cfg.roi.isValid(c.w, c.h)) {
            g.ax1 = cfg.roi.x1;
            g.ay1 = cfg.roi.y1;
            g.ax2 = cfg.roi.x2;
            g.ay2 = cfg.roi.y2;
            r.x1 = cfg.roi.x1;
            r.y1 = cfg.roi.y1;
            r.x2 = cfg.roi.x2;
            r.y2 = cfg.roi.y2;
        } else {
            g.ax2 = c.w;
            g.ay2 = c.h;
            r.x2 = c.w;
            r.y2 = c.h;
        }
        subtract = cfg.bgnd.on;
        if (subtract) {
            subtracted = QVector<double>(c.w*c.h);
            g.subtracted = subtracted.data();
        }

        normalize = cfg.plot.normalize;
    }

    void run() {
        qDebug() << "VirtualDemoCamera: started" << QThread::currentThreadId();
        start = QDateTime::currentDateTime();
        timer.start();
        while (true) {
            qint64 tm = timer.elapsed();
            if (tm - prevFrame < CAMERA_FRAME_DELAY_MS) {
                // Sleep gives a bad precision because OS decides how long the thread should sleep.
                // When we disable sleep, its possible to get an exact number of FPS,
                // e.g. 40 FPS when CAMERA_FRAME_DELAY_MS=25, but at cost of increased CPU usage.
                // Sleep allows to get about 30 FPS, which is enough, and relaxes CPU loading
                cam->msleep(CAMERA_LOOP_TICK_MS);
                continue;
            }

            avgFrameTime = avgFrameTime*0.9 + (tm - prevFrame)*0.1;
            prevFrame = tm;

            tm = timer.elapsed();
            cgn_render_beam_tilted(&b);
            avgRenderTime = avgRenderTime*0.9 + (timer.elapsed() - tm)*0.1;

            b.dx = dx_offset.next();
            b.dy = dy_offset.next();
            b.xc = xc_offset.next();
            b.yc = yc_offset.next();
            b.phi = phi_offset.next();

            tm = timer.elapsed();
            if (subtract) {
                cgn_calc_beam_bkgnd(&c, &g, &r);
            } else {
                cgn_calc_beam_naive(&c, &r);
                //cgn_calc_beam_blas(&c, &r);
            }

            saverMutex.lock();
            if (saver) {
                results->time = timer.elapsed();
                results->nan = r.nan;
                results->dx = r.dx;
                results->dy = r.dy;
                results->xc = r.xc;
                results->yc = r.yc;
                results->phi = r.phi;
                if (++resultIdx == MEASURE_BUF_SIZE) {
                    auto e = new MeasureEvent;
                    e->num = resultBufIdx;
                    e->count = MEASURE_BUF_SIZE;
                    e->results = resultBufs[resultBufIdx % MEASURE_BUF_COUNT];
                    e->start = start;
                    QCoreApplication::postEvent(saver, e);
                    results = resultBufs[++resultBufIdx % MEASURE_BUF_COUNT];
                    resultIdx = 0;
                } else {
                    results++;
                }
            }
            saverMutex.unlock();

            avgCalcTime = avgCalcTime*0.9 + (timer.elapsed() - tm)*0.1;

            if (tm - prevReady >= PLOT_FRAME_DELAY_MS) {
                prevReady = tm;
                if (subtract) {
                    if (normalize) {
                        cgn_copy_normalized_f64(g.subtracted, graph, c.w*c.h, g.min, g.max);
                    } else {
                        memcpy(graph, g.subtracted, sizeof(double)*c.w*c.h);
                    }
                } else {
                    if (normalize) {
                        cgn_render_beam_to_doubles_norm_8(c.buf, c.w*c.h, graph);
                    } else {
                        cgn_copy_to_f64(&c, graph, &g.max);
                    }
                }
                plot->invalidateGraph();
                if (normalize)
                    plot->setResult(r, 0, 1);
                else plot->setResult(r, g.min, g.max);
                table->setResult(r, avgRenderTime, avgCalcTime);
                emit cam->ready();
            }

            if (tm - prevStat >= STAT_DELAY_MS) {
                prevStat = tm;
                // TODO: average FPS over STAT_DELAY_MS
                CameraStats st {
                    .fps = qRound(1000.0/avgFrameTime),
                    .measureTime = measureStart > 0 ? timer.elapsed() - measureStart : -1,
                };
                emit cam->stats(st);
            #ifdef LOG_FRAME_TIME
                qDebug()
                    << "FPS:" << st.fps
                    << "avgFrameTime:" << qRound(avgFrameTime)
                    << "avgRenderTime:" << qRound(avgRenderTime)
                    << "avgCalcTime:" << qRound(avgCalcTime);
            #endif
                if (cam->isInterruptionRequested()) {
                    qDebug() << "VirtualDemoCamera: interrupted";
                    return;
                }
                if (reconfig) {
                    reconfig = false;
                    configure();
                    qDebug() << "VirtualDemoCamera: reconfigured";
                }
            }
        }
    }
};

VirtualDemoCamera::VirtualDemoCamera(PlotIntf *plot, TableIntf *table, QObject *parent) :
    Camera(plot, table, "VirtualDemoCamera"), QThread(parent)
{
    _render.reset(new BeamRenderer(plot, table, this));

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

void VirtualDemoCamera::startCapture()
{
    start();
}

void VirtualDemoCamera::startMeasure(QObject *saver)
{
    _render->saverMutex.lock();
    _render->resultIdx = 0;
    _render->resultBufIdx = 0;
    _render->results = _render->resultBufs[0];
    _render->measureStart = _render->timer.elapsed();
    _render->saver = saver;
    _render->saverMutex.unlock();
}

void VirtualDemoCamera::stopMeasure()
{
    _render->saverMutex.lock();
    _render->saver = nullptr;
    _render->measureStart = -1;
    _render->saverMutex.unlock();
}

void VirtualDemoCamera::run()
{
    _render->run();
}

void VirtualDemoCamera::camConfigChanged()
{
    _render->reconfig = true;
}
