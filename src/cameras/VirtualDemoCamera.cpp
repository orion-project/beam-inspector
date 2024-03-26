#include "VirtualDemoCamera.h"

#include "plot/BeamGraphIntf.h"

#include "beam_calc.h"
#include "beam_render.h"

#include <QDebug>
#include <QRandomGenerator>

#define CAMERA_LOOP_TICK_MS 5
#define CAMERA_FRAME_DELAY_MS 30
#define PLOT_FRAME_DELAY_MS 100
#define STAT_DELAY_MS 1000
// Calc and log how long it takes to process each frame (averaged)
//#define TRACK_FRAME_LEN

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
    QVector<uint8_t> d;
    RandomOffset dx_offset;
    RandomOffset dy_offset;
    RandomOffset xc_offset;
    RandomOffset yc_offset;
    RandomOffset phi_offset;
    qint64 prevFrame = 0;
    qint64 prevReady = 0;
    qint64 prevStat = 0;
    QSharedPointer<BeamGraphIntf> beam;
    VirtualDemoCamera *thread;
    double avgFrameTime = 0;
#ifdef TRACK_FRAME_LEN
    double avgRenderTime = 0;
    double avgCalcTime = 0;
#endif

    BeamRenderer(QSharedPointer<BeamGraphIntf> beam, VirtualDemoCamera *thread) : beam(beam), thread(thread)
    {
        b.w = 2592;
        b.h = 2048;
        b.dx = 1474;
        b.dy = 1120;
        b.xc = 1534;
        b.yc = 981;
        b.p0 = 255;
        b.phi = 12;
        d = QVector<uint8_t>(b.w * b.h);
        b.buf = d.data();

        c.w = b.w;
        c.h = b.h;
        c.buf = b.buf;
        if (cgn_calc_beam_init(&c)) {
            cgn_calc_beam_free(&c);
            qCritical() << "Failed to initialize calc buffers";
        }

        dx_offset = RandomOffset(b.dx, b.dx-20, b.dx+20);
        dy_offset = RandomOffset(b.dy, b.dy-20, b.dy+20);
        xc_offset = RandomOffset(b.xc, b.xc-20, b.xc+20);
        yc_offset = RandomOffset(b.yc, b.yc-20, b.yc+20);
        phi_offset = RandomOffset(b.phi, b.phi-12, b.phi+12);

        beam->init(b.w, b.h);
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

        #ifdef TRACK_FRAME_LEN
            tm = timer.elapsed();
        #endif
            cgn_render_beam_tilted(&b);
        #ifdef TRACK_FRAME_LEN
            avgRenderTime = avgRenderTime*0.9 + (timer.elapsed() - tm)*0.1;
        #endif

            b.dx = dx_offset.next();
            b.dy = dy_offset.next();
            b.xc = xc_offset.next();
            b.yc = yc_offset.next();
            b.phi = phi_offset.next();

        #ifdef TRACK_FRAME_LEN
            tm = timer.elapsed();
        #endif
            cgn_calc_beam(&c, &r);
        #ifdef TRACK_FRAME_LEN
            avgCalcTime = avgCalcTime*0.9 + (timer.elapsed() - tm)*0.1;
        #endif

            if (tm - prevReady >= PLOT_FRAME_DELAY_MS) {
                prevReady = tm;
                cgn_render_beam_to_doubles(&b, beam->rawData());
                beam->invalidate();
                emit thread->ready();
            }

            if (tm - prevStat >= STAT_DELAY_MS) {
                prevStat = tm;
                emit thread->stats(qRound(1000.0/avgFrameTime));
            #ifdef TRACK_FRAME_LEN
                qDebug() << "FPS:" << qRound(1000.0/avgFrameTime)
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

VirtualDemoCamera::VirtualDemoCamera(QSharedPointer<BeamGraphIntf> beam, QObject *parent) : QThread{parent}
{
    _render.reset(new BeamRenderer(beam, this));
}

void VirtualDemoCamera::run()
{
    _render->run();
}
