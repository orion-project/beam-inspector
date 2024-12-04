#ifndef CAMERA_WORKER
#define CAMERA_WORKER

#include "cameras/Camera.h"
#include "cameras/CameraTypes.h"
#include "cameras/MeasureSaver.h"
#include "widgets/PlotIntf.h"
#include "widgets/TableIntf.h"

#include "beam_calc.h"

#include <QApplication>
#include <QDebug>
#include <QMutex>
#include <QThread>

#define PLOT_FRAME_DELAY_MS 200
#define STAT_DELAY_MS 1000
#define MEASURE_BUF_SIZE 1000
#define MEASURE_BUF_COUNT 2

enum MeasureDataCol { COL_BRIGHTNESS, COL_POWER, COL_DEBUG_1, COL_DEBUG_2 };

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
    bool rawView = false;

    QDateTime start;
    QElapsedTimer timer;
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
    QList<CgnBeamResult> results;

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
    double brightness = 0;
    bool showBrightness = false;
    bool saveBrightness = false;
    bool showPower = false;
    bool hasPowerWarning = false;
    int calibratePowerFrames = 0;
    double calibratePowerTotal = 0;
    int powerDecimalFactor = 0;
    double power = 0;

    QMap<QString, QVariant> stats;
    std::function<QMap<int, CamTableData>()> tableData;

    const char *logId;

    CameraWorker(PlotIntf *plot, TableIntf *table, Camera *cam, QThread *thread, const char *logId)
        : plot(plot), table(table), camera(cam), thread(thread), logId(logId)
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

    inline void calcResult()
    {
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
                    results[i] = r;
                }
            } else {
                setRoi(roi);
                cgn_calc_beam_bkgnd(&c, &g, &r);
                results[0] = r;
            }
        }

        saverMutex.lock();
        if (calibratePowerFrames > 0) {
            if (multiRoi) {
                power = 0;
                if (results.size() > 0) {
                    for (const auto& r : results) {
                        power += r.p;
                    }
                    power /= double(results.size());
                }
            } else {
                power = r.p;
            }
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
            e->buf = QByteArray((const char*)c.buf, c.w*c.h*(c.bpp > 8 ? 2 : 1));
            QCoreApplication::postEvent(rawImgRequest, e);
            rawImgRequest = nullptr;
        }
        if (brightRequest) {
            auto e = new BrightEvent;
            e->level = cgn_calc_brightness_1(&c);
            QCoreApplication::postEvent(brightRequest, e);
            brightRequest = nullptr;
        }
        if (!rawView && saver) {
            qint64 time = timer.elapsed();
            if (saveImgInterval > 0 and (prevSaveImg == 0 or time - prevSaveImg >= saveImgInterval)) {
                prevSaveImg = time;
                auto e = new ImageEvent;
                e->time = time;
                e->buf = QByteArray((const char*)c.buf, c.w*c.h*(c.bpp > 8 ? 2 : 1));
                QCoreApplication::postEvent(saver, e);
            }
            measurs->time = time;
            measurs->nan = r.nan;
            measurs->dx = r.dx;
            measurs->dy = r.dy;
            measurs->xc = r.xc;
            measurs->yc = r.yc;
            measurs->phi = r.phi;
            if (saveBrightness)
                measurs->cols[COL_BRIGHTNESS] = cgn_calc_brightness_1(&c);
            if (showPower && calibratePowerFrames == 0)
                measurs->cols[COL_POWER] = power * powerScale;
            if (++measurIdx == MEASURE_BUF_SIZE ||
                (measureDuration > 0 && (time - measureStart >= measureDuration))) {
                sendMeasure();
            } else {
                measurs++;
            }
        }
        saverMutex.unlock();
    }

    inline void sendMeasure()
    {
        auto e = new MeasureEvent;
        e->num = measurBufIdx;
        e->count = measurIdx;
        e->results = measurBufs[measurBufIdx % MEASURE_BUF_COUNT];
        e->stats = stats;
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
            table->setResult({}, tableData());
            return true;
        }

        double minZ, maxZ;
        cgn_ext_copy_to_f64(&c, &g, graph, normalize, fullRange, &minZ, &maxZ);
        plot->invalidateGraph();
        plot->setResult(results, minZ, maxZ);

        table->setResult(results, tableData());
        return true;
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
        saver->setCaptureStart(start);
        saverMutex.unlock();
    }

    void stopMeasure()
    {
        saverMutex.lock();
        if (measurIdx > 0)
            sendMeasure();
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
