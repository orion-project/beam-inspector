#include "IdsComfortCamera.h"

#define LOG_ID "IdsComfortCamera:"

#include "app/AppSettings.h"
#include "cameras/CameraWorker.h"

#include "helpers/OriDialogs.h"

#include <ids_peak_comfort_c/ids_peak_comfort_c.h>

#include <QSettings>

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
        cam->_configGroup = cam->_name;
        cam->loadConfig();

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
                errCount++;
                stats[QStringLiteral("errorFrame")] = errCount;
                QString errKey = QStringLiteral("frameError_") + QString::number(res, 16);
                stats[errKey] = stats[errKey].toInt() + 1;
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
                    stats[QStringLiteral("underrunFrames")] = info.numUnderrun;
                    stats[QStringLiteral("droppedFrames")] = info.numDropped;
                    stats[QStringLiteral("incompleteFrames")] = info.numIncomplete;
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

void IdsComfortCamera::saveHardConfig(QSettings *s)
{
    if (!_peak)
        return;
    double v;
    auto res = peak_ExposureTime_Get(_peak->hCam, &v);
    if (PEAK_ERROR(res))
        s->setValue("exposure", getPeakError(res));
    else s->setValue("exposure", v);
    res = peak_FrameRate_Get(_peak->hCam, &v);
    if (PEAK_ERROR(res))
        s->setValue("frameRate", getPeakError(res));
    else s->setValue("frameRate", v);
}

void IdsComfortCamera::requestRawImg(QObject *sender)
{
    if (_peak)
        _peak->requestRawImg(sender);
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
#include <QWheelEvent>

#include "helpers/OriLayouts.h"
#include "widgets/OriValueEdit.h"

using namespace Ori::Layouts;
using namespace Ori::Widgets;

#define PROP_CONTROL(title, group, edit, label, setter) { \
    label = new QLabel; \
    label->setWordWrap(true); \
    label->setForegroundRole(qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark ? QPalette::Light : QPalette::Mid); \
    edit = new CamPropEdit; \
    edit->scrolled = [this](bool wheel, bool inc, bool big){ setter ## Fast(wheel, inc, big); }; \
    edit->connect(edit, &ValueEdit::keyPressed, [this](int key){ \
        if (key == Qt::Key_Return || key == Qt::Key_Enter) setter(); }); \
    auto btn = new QPushButton(tr("Set")); \
    btn->setFixedWidth(50); \
    btn->connect(btn, &QPushButton::pressed, [this]{ setter(); }); \
    group = LayoutV({label, LayoutH({edit, btn})}).makeGroupBox(title); \
    layout->addWidget(group); \
}

#define PROP_SHOW(method, id, edit, label, getValue, getRange, showAux) \
    void method() { \
        double value, min, max, step; \
        auto res = getValue(hCam, &value); \
        if (PEAK_ERROR(res)) { \
            label->setText(getPeakError(res)); \
            edit->setValue(0); \
            edit->setDisabled(true); \
            return; \
        } \
        edit->setValue(value); \
        edit->setDisabled(false); \
        props[id] = value; \
        res = getRange(hCam, &min, &max, &step); \
        if (PEAK_ERROR(res)) \
            label->setText(getPeakError(res)); \
        else { \
            label->setText(QString("<b>Min = %1, Max = %2</b>").arg(min, 0, 'f', 2).arg(max, 0, 'f', 2)); \
            props[id "_min"] = min; \
            props[id "_max"] = max; \
            props[id "_step"] = step; \
        }\
        showAux(value); \
    }

#define PROP_SET(method, id, edit, setValue) \
    void method() { \
        double v = edit->value(); \
        auto res = setValue(hCam, v); \
        if (PEAK_ERROR(res)) \
            Ori::Dlg::error(getPeakError(res)); \
        showExp(); \
        showFps(); \
    } \
    void method ## Fast(bool wheel, bool inc, bool big) { \
        double change = wheel \
            ? (big ? propChangeWheelBig : propChangeWheelSm) \
            : (big ? propChangeArrowBig : propChangeArrowSm); \
        double val = props[id]; \
        double newVal; \
        if (inc) { \
            double max = props[id "_max"]; \
            if (val >= max) return; \
            newVal = val * change; \
            newVal = qMin(max, val * change); \
        } else { \
            double min = props[id "_min"]; \
            if (val <= min) return; \
            newVal = val / change; \
            newVal = qMax(min, val / change); \
        } \
        double step = props[id "_step"]; \
        if (qAbs(val - newVal) < step) \
            newVal = val + (inc ? step : -step); \
        auto res = setValue(hCam, newVal); \
        if (PEAK_ERROR(res)) \
            Ori::Dlg::error(getPeakError(res)); \
        showExp(); \
        showFps(); \
    }

class CamPropEdit : public ValueEdit
{
public:
    std::function<void(bool, bool, bool)> scrolled;
protected:
    void keyPressEvent(QKeyEvent *e) override {
        if (e->key() == Qt::Key_Up) {
            scrolled(false, true, e->modifiers().testFlag(Qt::ControlModifier));
            e->accept();
        } else if (e->key() == Qt::Key_Down) {
            scrolled(false, false, e->modifiers().testFlag(Qt::ControlModifier));
            e->accept();
        } else {
            ValueEdit::keyPressEvent(e);
        }
    }
    void wheelEvent(QWheelEvent *e) override {
        scrolled(true, e->angleDelta().y() > 0, e->modifiers().testFlag(Qt::ControlModifier));
        e->accept();
    }
};

class IdsHardConfigWindow : public QWidget, public IAppSettingsListener
{
    Q_DECLARE_TR_FUNCTIONS(IdsHardConfigWindow)

public:
    IdsHardConfigWindow(peak_camera_handle hCam) : QWidget(qApp->activeWindow()), hCam(hCam)
    {
        setWindowFlag(Qt::Tool, true);
        setWindowFlag(Qt::WindowStaysOnTopHint, true);
        setAttribute(Qt::WA_DeleteOnClose, true);
        setWindowTitle(tr("Device Control"));

        applySettings();

        auto layout = new QVBoxLayout(this);

        PROP_CONTROL(tr("Exposure (us)"), groupExp, edExp, labExp, setExp)
        PROP_CONTROL(tr("Frame rate"), groupFps, edFps, labFps, setFps);

        labExpFreq = new QLabel;
        groupExp->layout()->addWidget(labExpFreq);

        if (peak_ExposureTime_GetAccessStatus(hCam) != PEAK_ACCESS_READWRITE) {
            labExp->setText(tr("Exposure is not configurable"));
            edExp->setDisabled(true);
        } else showExp();

        if (peak_FrameRate_GetAccessStatus(hCam) != PEAK_ACCESS_READWRITE) {
            labFps->setText(tr("FPS is not configurable"));
            edFps->setDisabled(true);
        } else showFps();
    }

    PROP_SET(setExp, "exp", edExp, peak_ExposureTime_Set)
    PROP_SET(setFps, "fps", edFps, peak_FrameRate_Set)

    PROP_SHOW(showExp, "exp", edExp, labExp, peak_ExposureTime_Get, peak_ExposureTime_GetRange, showExpFreq)
    PROP_SHOW(showFps, "fps", edFps, labFps, peak_FrameRate_Get, peak_FrameRate_GetRange, showAux)

    void showAux(double v) {}

    void showExpFreq(double exp)
    {
        double freq = 1e6 / exp;
        QString s;
        if (freq < 1000)
            s = QStringLiteral("Corresponds to <b>%1 Hz</b>").arg(freq, 0, 'f', 1);
        else
            s = QStringLiteral("Corresponds to <b>%1 kHz</b>").arg(freq/1000.0, 0, 'f', 2);
        labExpFreq->setText(s);
    }

    void applySettings()
    {
        auto &s = AppSettings::instance();
        propChangeWheelSm = 1 + double(s.propChangeWheelSm) / 100.0;
        propChangeWheelBig = 1 + double(s.propChangeWheelBig) / 100.0;
        propChangeArrowSm = 1 + double(s.propChangeArrowSm) / 100.0;
        propChangeArrowBig = 1 + double(s.propChangeArrowBig) / 100.0;
    }

    void settingsChanged() override
    {
        applySettings();
    }

    peak_camera_handle hCam;
    CamPropEdit *edExp, *edFps;
    QGroupBox *groupExp, *groupFps;
    QLabel *labExp, *labFps, *labExpFreq;
    QMap<const char*, double> props;
    double propChangeWheelSm, propChangeWheelBig;
    double propChangeArrowSm, propChangeArrowBig;
};

QPointer<QWidget> IdsComfortCamera::showHardConfgWindow()
{
    if (!_cfgWnd)
        _cfgWnd = new IdsHardConfigWindow(_peak->hCam);
    _cfgWnd->show();
    _cfgWnd->activateWindow();
    return _cfgWnd;
}
