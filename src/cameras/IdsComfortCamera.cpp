#include "IdsComfortCamera.h"

#define LOG_ID "IdsComfortCamera:"

#include "cameras/CameraWorker.h"

#include "helpers/OriDialogs.h"

#include <ids_peak_comfort_c/ids_peak_comfort_c.h>

#define FRAME_TIMEOUT 5000
//#define LOG_FRAME_TIME

// Implemented in IdsComfort.cpp
QString getPeakError(peak_status status);

#define CHECK_ERR(msg) \
    if (PEAK_ERROR(res)) { \
        auto err = getPeakError(res); \
        qCritical() << LOG_ID << msg << err; \
        return QString(msg ": ") + err; \
    }

#define SHOW_CAM_PROP(prop, func, typ) {\
    typ val; \
    auto res = func(hCam, &val); \
    if (PEAK_ERROR(res)) \
        qDebug() << LOG_ID << "Unable to get" << prop << getPeakError(res); \
    else qDebug() << LOG_ID << prop << val; \
}

class PeakIntf : public CameraWorker
{
public:
    peak_camera_id id;
    IdsComfortCamera *cam;
    peak_camera_handle hCam = PEAK_INVALID_HANDLE;
    peak_status res;
    peak_buffer buf;
    peak_frame_handle frame;
    int errCount = 0;

    PeakIntf(peak_camera_id id, PlotIntf *plot, TableIntf *table, IdsComfortCamera *cam)
        : CameraWorker(plot, table, cam, cam), id(id), cam(cam)
    {}

    QString init()
    {
        peak_camera_descriptor descr;
        res = peak_Camera_GetDescriptor(id, &descr);
        CHECK_ERR("Unable to get camera descriptor");
        cam->_name = QString::fromLatin1(descr.modelName);
        cam->_descr = cam->_name + ' ' + QString::fromLatin1(descr.serialNumber);

        res = peak_Camera_Open(id, &hCam);
        CHECK_ERR("Unable to open camera");
        qDebug() << LOG_ID << "Camera opened" << id;

        // TODO: support more depths
        peak_pixel_format targetFormat = PEAK_PIXEL_FORMAT_MONO8;
        //peak_pixel_format targetFormat = PEAK_PIXEL_FORMAT_MONO10G40_IDS;
        //peak_pixel_format targetFormat = PEAK_PIXEL_FORMAT_MONO12G24_IDS;
        peak_pixel_format pixelFormat = PEAK_PIXEL_FORMAT_INVALID;
        res = peak_PixelFormat_Get(hCam, &pixelFormat);
        CHECK_ERR("Unable to get pixel format");
        qDebug() << LOG_ID << "Pixel format" << QString::number(pixelFormat, 16);

        size_t formatCount = 0;
        res = peak_PixelFormat_GetList(hCam, nullptr, &formatCount);
        CHECK_ERR("Unable to get pixel format count");
        QVector<peak_pixel_format> pixelFormats(formatCount);
        res = peak_PixelFormat_GetList(hCam, pixelFormats.data(), &formatCount);
        CHECK_ERR("Unable to get pixel formats");
        bool supports = false;
        for (int i = 0; i < formatCount; i++) {
            qDebug() << LOG_ID << "Supported pixel format" << QString::number(pixelFormats[i], 16);
            if (pixelFormats[i] == targetFormat) {
                supports = true;
            }
        }
        if (!supports)
            // Use one of color formats and convert manually?
            return "Camera doesn't support gray scale format";
        if (pixelFormat != targetFormat) {
            res = peak_PixelFormat_Set(hCam, targetFormat);
            CHECK_ERR("Unable to set pixel format");
            qDebug() << LOG_ID << "pixel format set";
        }
        c.bits = 8;
        cam->_bits = c.bits;

        peak_roi roi = {0, 0, 0, 0};
        res = peak_ROI_Get(hCam, &roi);
        CHECK_ERR("Unable to get ROI");
        qDebug() << LOG_ID << QString("ROI size=%1x%2, offset=%3x%4")
            .arg(roi.size.width).arg(roi.size.height)
            .arg(roi.offset.x).arg(roi.offset.y);
        c.w = roi.size.width;
        c.h = roi.size.height;
        cam->_width = c.w;
        cam->_height = c.h;

        SHOW_CAM_PROP("FPS", peak_FrameRate_Get, double);
        SHOW_CAM_PROP("Exposure", peak_ExposureTime_Get, double);
        SHOW_CAM_PROP("PixelClock", peak_PixelClock_Get, double);

        res = peak_Acquisition_Start(hCam, PEAK_INFINITE);
        CHECK_ERR("Unable to start acquisition");
        qDebug() << LOG_ID << "Acquisition started";

        plot->initGraph(c.w, c.h);
        graph = plot->rawGraph();

        configure();

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
                qWarning() << LOG_ID << "Unable to stop acquisition";
            else qDebug() << LOG_ID << "Acquisition stopped";
        }

        auto res = peak_Camera_Close(hCam);
        if (PEAK_ERROR(res))
            qWarning() << LOG_ID << "Unable to close camera" << getPeakError(res);
        else qDebug() << LOG_ID << "Camera closed" << id;
        hCam = PEAK_INVALID_HANDLE;
    }

    void run()
    {
        qDebug() << LOG_ID << "Started" << QThread::currentThreadId();
        start = QDateTime::currentDateTime();
        timer.start();
        while (true) {
            if (waitFrame()) continue;

            tm = timer.elapsed();
            res = peak_Acquisition_WaitForFrame(hCam, FRAME_TIMEOUT, &frame);
            if (PEAK_SUCCESS(res))
                res = peak_Frame_Buffer_Get(frame, &buf);
            if (res == PEAK_STATUS_ABORTED) {
                auto err = getPeakError(res);
                qCritical() << LOG_ID << "Interrupted" << err;
                emit cam->error("Interrupted: " + err);
                return;
            }
            markRenderTime();

            if (res == PEAK_STATUS_SUCCESS) {
                if (buf.memorySize != c.w*c.h) {
                    qCritical() << LOG_ID << "Unexpected buffer size" << buf.memorySize;
                    emit cam->error("Invalid buffer size");
                    return;
                }

                tm = timer.elapsed();
                c.buf = buf.memoryAddress;
                calcResult();
                markCalcTime();

                if (showResults())
                    emit cam->ready();

                res = peak_Frame_Release(hCam, frame);
                if (PEAK_ERROR(res)) {
                    auto err = getPeakError(res);
                    qCritical() << LOG_ID << "Unable to release frame" << err;
                    emit cam->error("Unable to release frame: " + err);
                    return;
                }
            } else {
                // TODO: store errors of different types and show somehow
                errCount++;
            }

            if (tm - prevStat >= STAT_DELAY_MS) {
                prevStat = tm;

                QStringList errors;
                errors << QString::number(errCount);

                // TODO: suggest a way for sending arbitrary stats and showing it in the table
                peak_acquisition_info info;
                memset(&info, 0, sizeof(info));
                res = peak_Acquisition_GetInfo(hCam, &info);
                if (PEAK_SUCCESS(res)) {
                    errors << QString::number(info.numUnderrun)
                            << QString::number(info.numDropped)
                            << QString::number(info.numIncomplete);
                }

                double ft = avgFrameTime / avgFrameCount;
                avgFrameTime = 0;
                avgFrameCount = 0;
                CameraStats st {
                    .fps = qRound(1000.0/ft),
                    .measureTime = measureStart > 0 ? timer.elapsed() - measureStart : -1,
                    .errorFrames = errors.join(','),
                };
                emit cam->stats(st);
#ifdef LOG_FRAME_TIME
                qDebug()
                    << "FPS:" << st.fps
                    << "avgFrameTime:" << qRound(ft)
                    << "avgRenderTime:" << qRound(avgRenderTime)
                    << "avgCalcTime:" << qRound(avgCalcTime)
                    << "errCount: " << errCount
                    << getPeakError(res);
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

IdsComfortCamera::IdsComfortCamera(QVariant id, PlotIntf *plot, TableIntf *table, QObject *parent) :
    Camera(plot, table, "IdsComfortCamera"), QThread(parent), _id(id)
{
    auto peak = new PeakIntf(id.value<peak_camera_id>(), plot, table, this);
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
    if (_cfgWnd)
        _cfgWnd->deleteLater();

    qDebug() << LOG_ID << "Deleted";
}

PixelScale IdsComfortCamera::sensorScale() const
{
    return { .on=false }; // TODO: take from camera
}

void IdsComfortCamera::startCapture()
{
    start();
}

void IdsComfortCamera::stopCapture()
{
    if (_peak)
        _peak.reset(nullptr);
    if (_cfgWnd)
        _cfgWnd->deleteLater();
}

void IdsComfortCamera::startMeasure(MeasureSaver *saver)
{
    if (_peak)
        _peak->startMeasure(saver);
}

void IdsComfortCamera::stopMeasure()
{
    if (_peak)
        _peak->stopMeasure();
}

void IdsComfortCamera::run()
{
    if (_peak)
        _peak->run();
}

void IdsComfortCamera::camConfigChanged()
{
    if (_peak)
        _peak->reconfigure();
}

//------------------------------------------------------------------------------
//                             IdsHardConfigWindow
//------------------------------------------------------------------------------

#include <QApplication>
#include <QBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QStyleHints>

#include "helpers/OriLayouts.h"
#include "widgets/OriValueEdit.h"

using namespace Ori::Layouts;
using namespace Ori::Widgets;

class IdsHardConfigWindow : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(IdsHardConfigWindow)

public:
    IdsHardConfigWindow(peak_camera_handle hCam) : QWidget(qApp->activeWindow()), hCam(hCam)
    {
        setWindowFlag(Qt::Tool, true);
        setWindowFlag(Qt::WindowStaysOnTopHint, true);
        setAttribute(Qt::WA_DeleteOnClose, true);
        setWindowTitle(tr("Device Control"));

        auto layout = new QVBoxLayout(this);
        peak_status res;

        if (peak_ExposureTime_GetAccessStatus(hCam) != PEAK_ACCESS_READWRITE)
            qWarning() << LOG_ID << "Exposure is not configurable";
        else {
            double min, max, step;
            res = peak_ExposureTime_GetRange(hCam, &min, &max, &step);
            if (PEAK_ERROR(res))
                qWarning() << LOG_ID << "Unable to get exposure range" << getPeakError(res);
            else {
                qDebug() << LOG_ID << "Exposure range:" << min << max << step;
                auto label = new QLabel(QString("<b>Min = %1, Max = %2</b>").arg(min, 0, 'f', 2).arg(max, 0, 'f', 2));
                label->setForegroundRole(qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark ? QPalette::Light : QPalette::Mid);
                exposure = new ValueEdit;
                exposure->connect(exposure, &ValueEdit::keyPressed, [this](int key){
                    if (key == Qt::Key_Return || key == Qt::Key_Enter) setExposure();
                });
                auto b = new QPushButton(tr("Set"));
                b->setFixedWidth(50);
                b->connect(b, &QPushButton::pressed, [this]{ setExposure(); });
                layout->addWidget(LayoutV({label, LayoutH({exposure, b})}).makeGroupBox(tr("Exposure (us)")));
            }
        }
        if (peak_FrameRate_GetAccessStatus(hCam) != PEAK_ACCESS_READWRITE)
            qWarning() << LOG_ID << "FPS is not configurable";
        else {
            double min, max, step;
            res = peak_FrameRate_GetRange(hCam, &min, &max, &step);
            if (PEAK_ERROR(res))
                qWarning() << LOG_ID << "Unable to get FPS range" << getPeakError(res);
            else {
                qDebug() << LOG_ID << "FPS range:" << min << max << step;
                auto label = new QLabel(QString("<b>Min = %1, Max = %2</b>").arg(min, 0, 'f', 2).arg(max, 0, 'f', 2));
                label->setForegroundRole(qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark ? QPalette::Light : QPalette::Mid);
                fps = new ValueEdit;
                fps->connect(fps, &ValueEdit::keyPressed, [this](int key){
                    if (key == Qt::Key_Return || key == Qt::Key_Enter) setFps(); });
                auto b = new QPushButton(tr("Set"));
                b->setFixedWidth(50);
                b->connect(b, &QPushButton::pressed, [this]{ setFps(); });
                layout->addWidget(LayoutV({label, LayoutH({fps, b})}).makeGroupBox(tr("Frame rate")));
            }
        }
        showExposure();
        showFps();
    }

    void showExposure()
    {
        double v;
        auto res = peak_ExposureTime_Get(hCam, &v);
        if (PEAK_ERROR(res))
            qWarning() << LOG_ID << "Unable to get exposure" << getPeakError(res);
        else
            exposure->setValue(v);
    }

    void setExposure()
    {
        double v = exposure->value();
        auto res = peak_ExposureTime_Set(hCam, v);
        if (PEAK_ERROR(res))
            Ori::Dlg::error(getPeakError(res));
        showExposure();
        showFps();
    }

    void showFps()
    {
        double v;
        auto res = peak_FrameRate_Get(hCam, &v);
        if (PEAK_ERROR(res))
            qWarning() << LOG_ID << "Unable to get FPS" << getPeakError(res);
        else
            fps->setValue(v);
    }

    void setFps()
    {
        double v = fps->value();
        qDebug() << LOG_ID << "Set FPS" << v;
        auto res = peak_FrameRate_Set(hCam, v);
        if (PEAK_ERROR(res))
            Ori::Dlg::error(getPeakError(res));
        showExposure();
        showFps();
    }

    peak_camera_handle hCam;
    ValueEdit *exposure, *fps;
};

QPointer<QWidget> IdsComfortCamera::showHardConfgWindow()
{
    if (!_cfgWnd)
        _cfgWnd = new IdsHardConfigWindow(_peak->hCam);
    _cfgWnd->show();
    _cfgWnd->activateWindow();
    return _cfgWnd;
}
