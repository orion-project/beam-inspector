#ifndef CAMERA_WORKER
#define CAMERA_WORKER

#include "cameras/Camera.h"
#include "cameras/CameraTypes.h"
#include "cameras/MeasureSaver.h"
#include "widgets/PlotIntf.h"
#include "widgets/StabilityIntf.h"
#include "widgets/TableIntf.h"

#include "beam_calc.h"

#include <QApplication>
#include <QDebug>
#include <QMutex>
#include <QQueue>
#include <QThread>

#define PLOT_FRAME_DELAY_MS 200
#define STAT_DELAY_MS 1000
#define MEASURE_BUF_SIZE 1000
#define MEASURE_BUF_COUNT 2
#define SQR(x) ((x)*(x))

enum MeasureDataCol { COL_BRIGHTNESS, COL_POWER, COL_DEBUG_1, COL_DEBUG_2 };

inline int bytesSize(const CgnBeamCalc &c)
{
    return c.w * c.h * (c.bpp > 8 ? 2 : 1);
}

class CameraWorker
{
public:
    CgnBeamCalc c;
    CgnBeamResult r;
    CgnBeamBkgnd g;

    PlotIntf *plot;
    TableIntf *table;
    StabilityIntf *stabil;
    Camera *camera;
    QThread *thread;
    bool rawView = false;

    qint64 captureStart = 0;
    QElapsedTimer timer;
    /// Current frame time from the start of capturing @a captureStart
    qint64 tm;
    qint64 prevFrame = 0;
    qint64 prevReady = 0;
    qint64 prevStat = 0;
    qint64 prevSaveImg = 0;
    double avgFrameCount = 0;
    double avgFrameTime = 0;
    double avgAcqTime = 0;
    double avgCalcTime = 0;

    QMutex cfgMutex;
    bool subtract;
    bool normalize;
    bool fullRange;
    bool useRoi;
    bool multiRoi;
    bool reconfig = false;
    double powerScale = 0;
    RoiRect roi;
    QList<RoiRect> rois;
    double *graph;
    QVector<double> subtracted;
    /// Beam estimation results for each ROI, they are updated every frame.
    /// If the averaging is enabled, then results contain averaged values.
    QList<CgnBeamResult> results;
    QList<QQueue<CgnBeamResult>> mavgs;
    QList<CgnBeamResult> sdevs;

    MeasureSaver *saver = nullptr;
    QMutex saverMutex;
    QVector<Measurement> measurBuf1;
    QVector<Measurement> measurBuf2;
    Measurement *measurBufs[MEASURE_BUF_COUNT];
    Measurement *measurs;
    int measurIdx = 0;
    int measurBufIdx = 0;
    qint64 measureStart = -1;
    qint64 measureDuration = -1;
    qint64 saveImgInterval = 0;
    QObject *rawImgRequest = nullptr;
    QObject *brightRequest = nullptr;
    QObject *expWarningRequest = nullptr;
    double brightness = 0;
    bool showBrightness = false;
    bool saveBrightness = false;
    bool showPower = false;
    bool hasPowerWarning = false;
    int calibratePowerFrames = 0;
    double calibratePowerTotal = 0;
    int powerDecimalFactor = 0;
    double power = 0;
    double powerSdev = 0;
    bool doMavg = false;
    int mavgFrames = 0;

    QMap<QString, QVariant> stats;
    std::function<QMap<int, CamTableData>()> tableData;
    std::function<RawFrameData()> rawFrameData;

    const char *logId;

    CameraWorker(PlotIntf *plot, TableIntf *table, StabilityIntf *stabil, Camera *cam, QThread *thread, const char *logId)
        : plot(plot), table(table), stabil(stabil), camera(cam), thread(thread), logId(logId)
    {
        measurBuf1.resize(MEASURE_BUF_SIZE);
        measurBuf2.resize(MEASURE_BUF_SIZE);
        measurBufs[0] = measurBuf1.data();
        measurBufs[1] = measurBuf2.data();
        measurs = measurBufs[0];
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
        subtract = cfg.bgnd.on;
        if (subtract) {
            subtracted = QVector<double>(c.w*c.h);
            g.subtracted = subtracted.data();
        }
        normalize = cfg.plot.normalize;
        fullRange = cfg.plot.fullRange;
        multiRoi = cfg.roiMode == ROI_MULTI;
        useRoi = cfg.roiMode != ROI_NONE;
        roi = cfg.roi;
        rois = cfg.rois;
        results.resize(multiRoi ? rois.size() : 1);
        g.subtract_bkgnd_v = multiRoi ? 1 : 0;

        doMavg = cfg.mavg.on;
        mavgFrames = cfg.mavg.frames;
        if (doMavg) {
            mavgs.resize(multiRoi ? rois.size() : 1);
            sdevs.resize(multiRoi ? rois.size() : 1);
        } else {
            mavgs.clear();
            sdevs.clear();
        }
    }

    void setRoi(const RoiRect &roi)
    {
        if (useRoi && roi.isValid()) {
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
            qDebug() << logId << "Reconfigured";
        }
        cfgMutex.unlock();
    }

    inline void markAcqTime()
    {
        avgAcqTime = avgAcqTime*0.9 + (timer.elapsed() - tm)*0.1;
    }

    inline void markCalcTime()
    {
        avgCalcTime = avgCalcTime*0.9 + (timer.elapsed() - tm)*0.1;
    }

    inline void calcMavg(int roiIndex)
    {
        CgnBeamResult avg;
        memset(&avg, 0, sizeof(avg));
        QQueue<CgnBeamResult> &q = mavgs[roiIndex];
        q.enqueue(r);
        if (q.size() > mavgFrames) {
            const CgnBeamResult first = q.dequeue();
            const CgnBeamResult &last = r;
            const CgnBeamResult &prev = results.at(roiIndex);
            const double c = q.size();
            avg.xc = prev.xc + (last.xc - first.xc) / c;
            avg.yc = prev.yc + (last.yc - first.yc) / c;
            avg.dx = prev.dx + (last.dx - first.dx) / c;
            avg.dy = prev.dy + (last.dy - first.dy) / c;
            avg.phi = prev.phi + (last.phi - first.phi) / c;
            avg.p = prev.p + (last.p - first.p) / c;
        } else {
            for (const auto &r : q) {
                avg.xc += r.xc;
                avg.yc += r.yc;
                avg.dx += r.dx;
                avg.dy += r.dy;
                avg.phi += r.phi;
                avg.p += r.p;
            }
            const double c = q.size();
            avg.xc /= c;
            avg.yc /= c;
            avg.dx /= c;
            avg.dy /= c;
            avg.phi /= c;
            avg.p /= c;
        }
        results[roiIndex] = avg;

        CgnBeamResult sdev;
        memset(&sdev, 0, sizeof(sdev));
        for (const auto &r : q) {
            sdev.xc += SQR(r.xc - avg.xc);
            sdev.yc += SQR(r.yc - avg.yc);
            sdev.dx += SQR(r.dx - avg.dx);
            sdev.dy += SQR(r.dy - avg.dy);
            sdev.phi += SQR(r.phi - avg.phi);
            sdev.p += SQR(r.p - avg.p);
        }
        const double c = q.size();
        sdev.xc = qSqrt(sdev.xc / c);
        sdev.yc = qSqrt(sdev.yc / c);
        sdev.dx = qSqrt(sdev.dx / c);
        sdev.dy = qSqrt(sdev.dy / c);
        sdev.phi = qSqrt(sdev.phi / c);
        sdev.p = qSqrt(sdev.p / c);
        sdevs[roiIndex] = sdev;
    }

    inline void calcResult()
    {
        power = 0;
        powerSdev = 0;

        if (!rawView) {
            if (multiRoi) {
                if (subtract) {
                    g.min = 1e10;
                    g.max = -1e10;
                    cgn_copy_to_f64(&c, g.subtracted, nullptr);
                }
                for (int i = 0; i < rois.size(); i++) {
                    setRoi(rois.at(i));
                    cgn_calc_beam_bkgnd(&c, &g, &r);
                    if (doMavg) {
                        calcMavg(i);
                    } else {
                        results[i] = r;
                    }
                    if (showPower) {
                        power += results.at(i).p;
                        if (doMavg)
                            powerSdev += sdevs.at(i).p;
                    }
                }
                if (showPower) {
                    power /= double(rois.size());
                    if (doMavg)
                        powerSdev /= double(rois.size());
                }
            } else {
                setRoi(roi);
                cgn_calc_beam_bkgnd(&c, &g, &r);
                if (doMavg) {
                    calcMavg(0);
                } else {
                    results[0] = r;
                }
                if (showPower) {
                    power = results.at(0).p;
                    if (doMavg)
                        powerSdev = sdevs.at(0).p;
                }
            }
        }

        saverMutex.lock();
        if (calibratePowerFrames > 0) {
            qDebug() << logId << "calibrate power"
                 << "| step =" << calibratePowerFrames
                 << "| digital_intensity =" << power;
            calibratePowerTotal += power;
            if (--calibratePowerFrames == 0) {
                hasPowerWarning = false;
                calibratePowerTotal /= double(camera->config().power.avgFrames);
                powerScale = camera->config().power.power / calibratePowerTotal;
                qDebug() << logId << "calibrate power"
                    << "| digital_intensity_avg =" << calibratePowerTotal
                    << "| power =" << camera->config().power.power
                    << "| scale =" << powerScale;
            }
        }
        if (rawImgRequest) {
            auto e = new ImageEvent;
            e->time = 0;
            e->buf = QByteArray((const char*)c.buf, bytesSize(c));
            if (rawFrameData)
                e->raw = rawFrameData();
            QCoreApplication::postEvent(rawImgRequest, e);
            rawImgRequest = nullptr;
        }
        if (brightRequest) {
            auto e = new BrightEvent;
            e->level = cgn_calc_brightness_1(&c);
            QCoreApplication::postEvent(brightRequest, e);
            brightRequest = nullptr;
        }
        if (expWarningRequest && !saver) {
            auto e = new ExpWarningEvent;
            e->overexposed = cgn_calc_overexposure(&c, 0.8);
            QCoreApplication::postEvent(expWarningRequest, e);
            expWarningRequest = nullptr;
        }
        if (!rawView && saver) {
            if (saveImgInterval > 0 and (prevSaveImg == 0 or tm - prevSaveImg >= saveImgInterval)) {
                prevSaveImg = tm;
                auto e = new ImageEvent;
                e->time = frameTimeAbs();
                e->buf = QByteArray((const char*)c.buf, bytesSize(c));
                QCoreApplication::postEvent(saver, e);
            }
            measurs->time = frameTimeAbs();
            if (multiRoi) {
                for (int i = 0; i < results.size(); i++) {
                    const auto &r = results.at(i);
                    measurs->cols[MULTIRES_IDX_NAN(i)] = r.nan ? 1 : 0;
                    measurs->cols[MULTIRES_IDX_DX(i)] = r.dx;
                    measurs->cols[MULTIRES_IDX_DY(i)] = r.dy;
                    measurs->cols[MULTIRES_IDX_XC(i)] = r.xc;
                    measurs->cols[MULTIRES_IDX_YC(i)] = r.yc;
                    measurs->cols[MULTIRES_IDX_PHI(i)] = r.phi;
                }
            } else {
                measurs->nan = r.nan;
                measurs->dx = r.dx;
                measurs->dy = r.dy;
                measurs->xc = r.xc;
                measurs->yc = r.yc;
                measurs->phi = r.phi;
            }
            if (saveBrightness)
                measurs->cols[COL_BRIGHTNESS] = cgn_calc_brightness_1(&c);
            if (showPower && calibratePowerFrames == 0)
                measurs->cols[COL_POWER] = power * powerScale;
            measurIdx++;
            if (measureDuration > 0 && (tm - measureStart >= measureDuration)) {
                sendMeasure(true, true);
                saver = nullptr;
            }
            if (measurIdx == MEASURE_BUF_SIZE) {
                sendMeasure(false, false);
            } else {
                measurs++;
            }
        }
        saverMutex.unlock();
    }

    inline void sendMeasure(bool last, bool finished)
    {
        auto e = new MeasureEvent;
        e->num = measurBufIdx;
        e->count = measurIdx;
        e->results = measurBufs[measurBufIdx % MEASURE_BUF_COUNT];
        e->stats = stats;
        e->last = last;
        e->finished = finished;
        QCoreApplication::postEvent(saver, e);
        measurs = measurBufs[++measurBufIdx % MEASURE_BUF_COUNT];
        measurIdx = 0;
    }

    inline bool showResults()
    {
        if (tm - prevReady < PLOT_FRAME_DELAY_MS)
            return false;
        prevReady = tm;
        const double rangeTop = (1 << c.bpp) - 1;

        if (showBrightness)
            brightness = cgn_calc_brightness_1(&c);

        if (rawView)
        {
            cgn_copy_to_f64(&c, graph, &g.max);
            plot->invalidateGraph();
            plot->setResult({}, 0, rangeTop);
            table->setResult({}, {}, tableData());
            stabil->setResult(frameTimeAbs(), {});
            return true;
        }

        double minZ, maxZ;
        cgn_ext_copy_to_f64(&c, &g, graph, normalize, fullRange, &minZ, &maxZ);
        plot->invalidateGraph();
        plot->setResult(results, minZ, maxZ);

        table->setResult(results, sdevs, tableData());
        
        // Stability plotter accepts the latest instant results (not averaged)
        if (doMavg) {
            QList<CgnBeamResult> res;
            if (multiRoi) {
                for (int i = 0; i < rois.size(); i++) {
                    const QQueue<CgnBeamResult> &q = mavgs.at(i);
                    if (q.isEmpty())
                        res << CgnBeamResult { .nan = true };
                    else res << q.last();
                }
            } else {
                const QQueue<CgnBeamResult> &q = mavgs.at(0);
                if (q.isEmpty())
                    res << CgnBeamResult { .nan = true };
                else res << q.last();
            }
            stabil->setResult(frameTimeAbs(), res);
        } else {
            stabil->setResult(frameTimeAbs(), results);
        }
        
        return true;
    }
    
    void startCapture()
    {
        qDebug() << logId << "Started" << QThread::currentThreadId();
        captureStart = QDateTime::currentMSecsSinceEpoch();
        timer.start();
    }
    
    inline qint64 frameTimeAbs()
    {
        return captureStart + tm;
    }

    void startMeasure(MeasureSaver *s)
    {
        saverMutex.lock();
        measurIdx = 0;
        measurBufIdx = 0;
        measurs = measurBufs[0];
        measureStart = timer.elapsed();
        measureDuration = s->config().durationInf ? -1 : s->config().durationSecs() * 1000;
        saveImgInterval = s->config().saveImg ? s->config().imgIntervalSecs() * 1000 : 0;
        saver = s;
        saverMutex.unlock();
    }

    void stopMeasure()
    {
        saverMutex.lock();
        if (measurIdx > 0)
            sendMeasure(true, false);
        saver = nullptr;
        measureStart = -1;
        measureDuration = -1;
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

    void requestExpWarning(QObject *sender)
    {
        saverMutex.lock();
        expWarningRequest = sender;
        saverMutex.unlock();
    }

    void setRawView(bool on, bool reconfig)
    {
        saverMutex.lock();
        if (rawView != on) {
            rawView = on;
            if (reconfig)
                this->reconfig = true;
        }
        saverMutex.unlock();
    }

    void togglePowerMeter() {
        saverMutex.lock();
        showPower = camera->config().power.on;
        if (showPower) {
            powerDecimalFactor = camera->config().power.decimalFactor;
            calibratePowerTotal = 0;
            calibratePowerFrames = std::clamp(camera->config().power.avgFrames, PowerMeter::minAvgFrames, PowerMeter::maxAvgFrames);
        }
        saverMutex.unlock();
    }
};

#endif // CAMERA_WORKER
