#ifndef CAMERA_WORKER
#define CAMERA_WORKER

#include "cameras/Camera.h"
#include "cameras/CameraTypes.h"
#include "cameras/MeasureSaver.h"
#include "widgets/PlotIntf.h"
#include "widgets/TableIntf.h"

#include "beam_calc.h"
#include "beam_render.h"

#include <QApplication>
#include <QDebug>
#include <QMutex>

#define CAMERA_LOOP_TICK_MS 5
#define CAMERA_FRAME_DELAY_MS 30
#define PLOT_FRAME_DELAY_MS 200
#define STAT_DELAY_MS 1000
#define MEASURE_BUF_SIZE 1000
#define MEASURE_BUF_COUNT 2

class CameraWorker
{
public:
    CgnBeamCalc c;
    CgnBeamResult r;
    CgnBeamBkgnd g;

    PlotIntf *plot;
    TableIntf *table;
    Camera *camera;
    QThread *thread;

    QDateTime start;
    QElapsedTimer timer;
    qint64 tm;
    qint64 prevFrame = 0;
    qint64 prevReady = 0;
    qint64 prevStat = 0;
    qint64 prevSaveImg = 0;
    double avgFrameCount = 0;
    double avgFrameTime = 0;
    double avgRenderTime = 0;
    double avgCalcTime = 0;

    QMutex cfgMutex;
    bool subtract;
    bool normalize;
    bool reconfig = false;

    double *graph;
    QVector<double> subtracted;

    MeasureSaver *saver = nullptr;
    QMutex saverMutex;
    QVector<Measurement> resultBuf1;
    QVector<Measurement> resultBuf2;
    Measurement *resultBufs[MEASURE_BUF_COUNT];
    Measurement *results;
    int resultIdx = 0;
    int resultBufIdx = 0;
    qint64 measureStart = -1;
    qint64 saveImgInterval = 0;
    QObject *rawImgRequest = nullptr;
    QObject *brightRequest = nullptr;

    QMap<QString, QVariant> stats;

    CameraWorker(PlotIntf *plot, TableIntf *table, Camera *cam, QThread *thread)
        : plot(plot), table(table), camera(cam), thread(thread)
    {
        resultBuf1.resize(MEASURE_BUF_SIZE);
        resultBuf2.resize(MEASURE_BUF_SIZE);
        resultBufs[0] = resultBuf1.data();
        resultBufs[1] = resultBuf2.data();
        results = resultBufs[0];
    }

    void configure()
    {
        reconfig = false;

        memset(&r, 0, sizeof(CgnBeamResult));
        memset(&g, 0, sizeof(CgnBeamBkgnd));

        auto cfg = camera->config();
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

    void reconfigure()
    {
        cfgMutex.lock();
        reconfig = true;
        cfgMutex.unlock();
    }

    void checkReconfig()
    {
        cfgMutex.lock();
        if (reconfig) {
            configure();
            qDebug() << LOG_ID << "Reconfigured";
        }
        cfgMutex.unlock();
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

    inline void markRenderTime()
    {
        avgRenderTime = avgRenderTime*0.9 + (timer.elapsed() - tm)*0.1;
    }

    inline void markCalcTime()
    {
        avgCalcTime = avgCalcTime*0.9 + (timer.elapsed() - tm)*0.1;
    }

    inline void calcResult()
    {
        if (subtract) {
            cgn_calc_beam_bkgnd(&c, &g, &r);
        } else {
            cgn_calc_beam_naive(&c, &r);
        }

        saverMutex.lock();
        if (rawImgRequest) {
            auto e = new ImageEvent;
            e->time = 0;
            e->buf = QByteArray((const char*)c.buf, c.w*c.h*(c.bits>>3));
            QCoreApplication::postEvent(rawImgRequest, e);
            rawImgRequest = nullptr;
        }
        if (brightRequest) {
            auto e = new BrightEvent;
            e->level = cgn_calc_brightness(&c);
            QCoreApplication::postEvent(brightRequest, e);
            brightRequest = nullptr;
        }
        if (saver) {
            qint64 time = timer.elapsed();
            if (saveImgInterval > 0 and (prevSaveImg == 0 or time - prevSaveImg >= saveImgInterval)) {
                prevSaveImg = time;
                auto e = new ImageEvent;
                e->time = time;
                e->buf = QByteArray((const char*)c.buf, c.w*c.h*(c.bits>>3));
                QCoreApplication::postEvent(saver, e);
            }
            results->time = time;
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
                e->stats = stats;
                QCoreApplication::postEvent(saver, e);
                results = resultBufs[++resultBufIdx % MEASURE_BUF_COUNT];
                resultIdx = 0;
            } else {
                results++;
            }
        }
        saverMutex.unlock();
    }

    inline bool showResults()
    {
        if (tm - prevReady < PLOT_FRAME_DELAY_MS)
            return false;
        prevReady = tm;
        if (subtract)
        {
            if (normalize)
                cgn_copy_normalized_f64(g.subtracted, graph, c.w*c.h, g.min, g.max);
            else
                memcpy(graph, g.subtracted, sizeof(double)*c.w*c.h);
        }
        else
        {
            if (normalize)
                cgn_render_beam_to_doubles_norm_8(c.buf, c.w*c.h, graph);
            else
                cgn_copy_to_f64(&c, graph, &g.max);
        }
        plot->invalidateGraph();
        if (normalize)
            plot->setResult(r, 0, 1);
        else plot->setResult(r, g.min, g.max);
        //else plot->setResult(r, 0, (1 << c.bits)-1);
        table->setResult(r, avgRenderTime, avgCalcTime);
        return true;
    }

    void startMeasure(MeasureSaver *s)
    {
        saverMutex.lock();
        resultIdx = 0;
        resultBufIdx = 0;
        results = resultBufs[0];
        measureStart = timer.elapsed();
        saveImgInterval = s->config().saveImg ? s->config().imgIntervalSecs() * 1000 : 0;
        saver = s;
        saver->setCaptureStart(start);
        saverMutex.unlock();
    }

    void stopMeasure()
    {
        saverMutex.lock();
        saver = nullptr;
        measureStart = -1;
        saverMutex.unlock();
    }

    void requestRawImg(QObject *sender)
    {
        saverMutex.lock();
        rawImgRequest = sender;
        saverMutex.unlock();
    }

    void requestBrightness(QObject *sender)
    {
        saverMutex.lock();
        brightRequest = sender;
        saverMutex.unlock();
    }
};

#endif // CAMERA_WORKER
