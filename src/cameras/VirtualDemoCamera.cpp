#include "VirtualDemoCamera.h"

#include "widgets/PlotIntf.h"
#include "widgets/TableIntf.h"

#include "beam_calc.h"
#include "beam_render.h"

#include <QDebug>
#include <QRandomGenerator>

#define CAMERA_LOOP_TICK_MS 5
#define CAMERA_FRAME_DELAY_MS 30
#define PLOT_FRAME_DELAY_MS 200
#define STAT_DELAY_MS 1000
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
    VirtualDemoCamera *thread;
    double avgFrameTime = 0;
    double avgRenderTime = 0;
    double avgCalcTime = 0;
    QVector<double> subtracted;
    bool subtract;
    bool normalize;
    double *graph;

    BeamRenderer(PlotIntf *plot, TableIntf *table, VirtualDemoCamera *thread) : plot(plot), table(table), thread(thread)
    {
        auto settings = CameraSettings();
        settings.load("StillImageCamera");
        settings.print();

        auto info = VirtualDemoCamera::info();

        b.w = info.width;
        b.h = info.height;
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

        g.iters = 0;
        g.max_iter = settings.maxIters;
        g.precision = settings.precision;
        g.corner_fraction = settings.cornerFraction;
        g.nT = settings.nT;
        g.mask_diam = settings.maskDiam;
        g.min = 0;
        g.max = 0;
        g.ax1 = 0, g.ax2 = c.w;
        g.ay1 = 0, g.ay2 = c.h;
        subtract = settings.subtractBackground;
        if (subtract) {
            subtracted = QVector<double>(c.w*c.h);
            g.subtracted = subtracted.data();
        }

        normalize = settings.normalize;

        plot->initGraph(b.w, b.h);
        graph = plot->rawGraph();
    }

    void run() {
        QElapsedTimer timer;
        timer.start();
        while (true) {
            qint64 tm = timer.elapsed();
            if (tm - prevFrame < CAMERA_FRAME_DELAY_MS) {
                // Sleep gives a bad precision because OS decides how long the thread should sleep.
                // When we disable sleep, its possible to get an exact number of FPS,
                // e.g. 40 FPS when CAMERA_FRAME_DELAY_MS=25, but at cost of increased CPU usage.
                // Sleep allows to get about 30 FPS, which is enough, and relaxes CPU loading
                thread->msleep(CAMERA_LOOP_TICK_MS);
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
            avgCalcTime = avgCalcTime*0.9 + (timer.elapsed() - tm)*0.1;

            if (tm - prevReady >= PLOT_FRAME_DELAY_MS) {
                prevReady = tm;
                if (subtract) {
                    if (normalize) {
                        cgn_copy_normalized_f64(g.subtracted, graph, c.w*c.h, g.min, g.max);
                    } else {
                        memcpy(plot, g.subtracted, sizeof(double)*c.w*c.h);
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
                emit thread->ready();
            }

            if (tm - prevStat >= STAT_DELAY_MS) {
                prevStat = tm;
                // TODO: average FPS over STAT_DELAY_MS
                emit thread->stats(qRound(1000.0/avgFrameTime));
            #ifdef LOG_FRAME_TIME
                qDebug()
                    << "FPS:" << qRound(1000.0/avgFrameTime)
                    << "avgFrameTime:" << qRound(avgFrameTime)
                    << "avgRenderTime:" << qRound(avgRenderTime)
                    << "avgCalcTime:" << qRound(avgCalcTime);
            #endif
                if (thread->isInterruptionRequested()) {
                    qDebug() << "VirtualDemoCamera::interrupted";
                    return;
                }
            }
        }
    }
};

VirtualDemoCamera::VirtualDemoCamera(PlotIntf *plot, TableIntf *table, QObject *parent) : QThread{parent}
{
    _render.reset(new BeamRenderer(plot, table, this));
}

void VirtualDemoCamera::run()
{
    _render->run();
}

CameraInfo VirtualDemoCamera::info()
{
    return CameraInfo {
        .name = "Camera: VirtualDemo",
        .width = 2592,
        .height = 2048,
        .bits = 8,
    };
}
