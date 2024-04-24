#include "IdsComfortCamera.h"

#include "widgets/PlotIntf.h"
#include "widgets/TableIntf.h"

#include "helpers/OriDialogs.h"

#include "beam_calc.h"
#include "beam_render.h"

#include <ids_peak_comfort_c/ids_peak_comfort_c.h>

#include <QDebug>

#define CAMERA_LOOP_TICK_MS 5
#define CAMERA_FRAME_DELAY_MS 30
#define PLOT_FRAME_DELAY_MS 200
#define STAT_DELAY_MS 1000
#define MEASURE_BUF_SIZE 1000
#define MEASURE_BUF_COUNT 2
#define FRAME_TIMEOUT 5000
//#define LOG_FRAME_TIME

static peak_camera_handle hCam = PEAK_INVALID_HANDLE;

static QString getPeakError(peak_status status)
{
    size_t bufSize = 1024;
    QByteArray buf(bufSize, 0);
    auto res = peak_Library_GetLastError(&status, buf.data(), &bufSize);
    if (PEAK_SUCCESS(res) == PEAK_TRUE) {
        int sz = 0;
        for (int i = 0; i < bufSize; i++)
            if (buf.at(i) == 0) {
                sz = i;
                break;
            }
        return QString::fromLatin1(buf.data(), sz);
    }
    if (res == PEAK_STATUS_BUFFER_TOO_SMALL) {
        qWarning() << "Unable to get text for error" << status << "because of buffer is too small";
        return QString("errcode=%1").arg(status);
    }
    qWarning() << "Unable to get text for error" << status << "because of error" << res;
    return QString("errcode=%1").arg(status);
}

#define CHECK_ERR(msg) \
    if (PEAK_ERROR(res)) { \
        auto err = getPeakError(res); \
        qWarning() << "IdsComfortCamera:" << msg << err; \
        return QString(msg ": ") + err; \
    }

class PeakIntf
{
public:
    CgnBeamResult r;
    PlotIntf *plot;
    TableIntf *table;
    IdsComfortCamera *cam;
    int w, h, bits;
    QDateTime start;
    QElapsedTimer timer;
    qint64 measureStart = -1;
    qint64 prevFrame = 0;
    qint64 prevReady = 0;
    qint64 prevStat = 0;
    double avgFrameTime = 0;
    double avgRenderTime = 0;
    double avgCalcTime = 0;
    peak_status res;
    peak_buffer buf;
    peak_frame_handle frame;
    int errCount;
    double *graph;

    QString init()
    {
        if (hCam != PEAK_INVALID_HANDLE)
            return "Already initialized";

        res = peak_Library_Init();
        CHECK_ERR("Unable to init library");
        qDebug() << "IdsComfortCamera: library inited";

        res = peak_CameraList_Update(nullptr);
        CHECK_ERR("Unable to update camera list");
        qDebug() << "IdsComfortCamera: camera list updated";

        res = peak_Camera_OpenFirstAvailable(&hCam);
        CHECK_ERR("Unable to open first camera");
        qDebug() << "IdsComfortCamera: camera opened";

        peak_pixel_format targetFormat = PEAK_PIXEL_FORMAT_MONO8;
        //peak_pixel_format targetFormat = PEAK_PIXEL_FORMAT_BAYER_GB8;
        peak_pixel_format pixelFormat = PEAK_PIXEL_FORMAT_INVALID;
        res = peak_PixelFormat_Get(hCam, &pixelFormat);
        CHECK_ERR("Unable to get pixel format");
        qDebug() << "IdsComfortCamera: pixel format" << QString::number(pixelFormat, 16);

        // TODO: support more depth
        if (pixelFormat != targetFormat) {
            size_t pixelFormatCount = 100;
            peak_pixel_format pixelFormats[100];
            memset(pixelFormats, 0, sizeof(peak_pixel_format)*100);
            res = peak_PixelFormat_GetList(hCam, pixelFormats, &pixelFormatCount);
            CHECK_ERR("Unable to get pixel formats");
            bool supports = false;
            for (int i = 0; i < pixelFormatCount; i++)
                if (pixelFormats[i] == targetFormat) {
                    supports = true;
                    break;
                }
            if (!supports)
                // Use one of color formats and convert manually?
                return "Camera doesn't support gray scale format";
            res = peak_PixelFormat_Set(hCam, targetFormat);
            CHECK_ERR("Unable to set pixel format");
            qDebug() << "IdsComfortCamera: pixel format set";
        }
        bits = 8;

        peak_roi roi = {0, 0, 0, 0};
        res = peak_ROI_Get(hCam, &roi);
        CHECK_ERR("Unable to get ROI");
        qDebug() << "IdsComfortCamera: ROI "
                 << "| w =" << roi.size.width << "| h =" << roi.size.height
                 << "| x =" << roi.offset.x << "| y =" << roi.offset.y;
        w = roi.size.width;
        h = roi.size.height;

        res = peak_Acquisition_Start(hCam, PEAK_INFINITE);
        CHECK_ERR("Unable to start acquisition");
        qDebug() << "IdsComfortCamera: acquisition started";

        plot->initGraph(w, h);
        graph = plot->rawGraph();

        memset(&r, 0, sizeof(CgnBeamResult));

        return {};
    }

    ~PeakIntf()
    {
        if (hCam == PEAK_INVALID_HANDLE)
            return;

        if (peak_Acquisition_IsStarted(hCam))
        {
            auto res = peak_Acquisition_Stop(hCam);
            if (PEAK_ERROR(res))
                qWarning() << "IdsComfortCamera: unable to stop acquisition";
            else qDebug() << "IdsComfortCamera: acquisition stopped";
        }

        auto res = peak_Camera_Close(hCam);
        if (PEAK_ERROR(res))
            qWarning() << "IdsComfortCamera: unable to close camera" << getPeakError(res);
        else qDebug() << "IdsComfortCamera: camera closed";
        hCam = PEAK_INVALID_HANDLE;

        res = peak_Library_Exit();
        if (PEAK_ERROR(res))
            qWarning() << "IdsComfortCamera: unable to deinit library" << getPeakError(res);
        else qDebug() << "IdsComfortCamera: library deinited";
    }

    void run()
    {
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
            res = peak_Acquisition_WaitForFrame(hCam, FRAME_TIMEOUT, &frame);
            if (PEAK_SUCCESS(res))
                res = peak_Frame_Buffer_Get(frame, &buf);
            if (res == PEAK_STATUS_ABORTED) {
                auto err = getPeakError(res);
                qDebug() << "IdsComfortCamera: interrupted" << err;
                emit cam->error("Interrupted: " + err);
                return;
            }
            avgRenderTime = avgRenderTime*0.9 + (timer.elapsed() - tm)*0.1;

            if (res == PEAK_STATUS_SUCCESS) {
                tm = timer.elapsed();
                if (!processFrame()) {
                    return;
                }
                // TODO: measure
                avgCalcTime = avgCalcTime*0.9 + (timer.elapsed() - tm)*0.1;

                res = peak_Frame_Release(hCam, frame);
                if (PEAK_ERROR(res)) {
                    auto err = getPeakError(res);
                    qWarning() << "IdsComfortCamera: unable to release frame" << err;
                    emit cam->error("Unable to release frame: " + err);
                    return;
                }

                if (tm - prevReady >= PLOT_FRAME_DELAY_MS) {
                    prevReady = tm;
                    cgn_render_beam_to_doubles_norm_8(buf.memoryAddress, buf.memorySize, graph);
                    /*if (subtract) {
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
                }*/
                    plot->invalidateGraph();
                    /*if (normalize)
                        plot->setResult(r, 0, 1);
                        else plot->setResult(r, g.min, g.max);*/
                    plot->setResult(r, 0, 1);
                    table->setResult(r, avgRenderTime, avgCalcTime);
                    emit cam->ready();
                }

            }
            else {
                errCount++;
            }

            if (tm - prevStat >= STAT_DELAY_MS) {
                prevStat = tm;
                // TODO: average FPS over STAT_DELAY_MS
                emit cam->stats(qRound(1000.0/avgFrameTime), measureStart > 0 ? timer.elapsed() - measureStart : -1);
#ifdef LOG_FRAME_TIME
                qDebug()
                    << "FPS:" << qRound(1000.0/avgFrameTime)
                    << "avgFrameTime:" << qRound(avgFrameTime)
                    << "avgRenderTime:" << qRound(avgRenderTime)
                    << "avgCalcTime:" << qRound(avgCalcTime)
                    << "errCount: " << errCount
                    << getPeakError(res);
#endif
                if (cam->isInterruptionRequested()) {
                    qDebug() << "IdsComfortCamera: interrupted";
                    return;
                }
            }
        }
    }

    bool processFrame()
    {
        if (buf.memorySize != w*h) {
            emit cam->error("Invalid buffer size");
            return false;
        }
        // TODO calc
        return true;
    }
};

IdsComfortCamera::IdsComfortCamera(PlotIntf *plot, TableIntf *table, QObject *parent) :
    Camera(plot, table, "IdsComfortCamera"), QThread(parent)
{
    auto peak = new PeakIntf;
    peak->plot = plot;
    peak->table = table;
    peak->cam = this;
    auto res = peak->init();
    if (!res.isEmpty())
    {
        Ori::Dlg::error(res);
        delete peak;
        return;
    }
    _peak.reset(peak);

    connect(parent, SIGNAL(camConfigChanged()), this, SLOT(camConfigChanged()));
}

IdsComfortCamera::~IdsComfortCamera()
{
    qDebug() << "IdsComfortCamera: deleted";
}

QString IdsComfortCamera::name() const
{
    return "IDS"; // TODO: take from camera
}

int IdsComfortCamera::width() const
{
    return _peak ? _peak->w : 0;
}

int IdsComfortCamera::height() const
{
    return _peak ? _peak->h : 0;
}

int IdsComfortCamera::bits() const
{
    return _peak ? _peak->bits : 0;
}

PixelScale IdsComfortCamera::sensorScale() const
{
    return { .on=false }; // TODO: take from camera
}

void IdsComfortCamera::startCapture()
{
    start();
}

void IdsComfortCamera::startMeasure(QObject *saver)
{
    // TODO
}

void IdsComfortCamera::stopMeasure()
{
    // TODO
}

void IdsComfortCamera::run()
{
    if (_peak)
        _peak->run();
}

void IdsComfortCamera::camConfigChanged()
{
    // TODO
}
