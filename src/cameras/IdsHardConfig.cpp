#include "IdsHardConfig.h"

#ifdef WITH_IDS

#include "app/AppSettings.h"
#include "cameras/CameraTypes.h"
#include "cameras/IdsLib.h"

#include "helpers/OriLayouts.h"
#include "helpers/OriDialogs.h"
#include "tools/OriSettings.h"
#include "widgets/OriValueEdit.h"

#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QClipboard>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStyleHints>
#include <QWheelEvent>

#define LOG_ID "IdsHardConfig:"

using namespace Ori::Layouts;
using namespace Ori::Widgets;

#define PROP_CONTROL(Prop, title) { \
    auto label = new QLabel; \
    label->setWordWrap(true); \
    label->setForegroundRole(qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark ? QPalette::Light : QPalette::Mid); \
    auto edit = new CamPropEdit; \
    edit->scrolled = [this](bool wheel, bool inc, bool big){ set##Prop##Fast(wheel, inc, big); }; \
    edit->connect(edit, &ValueEdit::keyPressed, edit, [this](int key){ \
        if (key == Qt::Key_Return || key == Qt::Key_Enter) set##Prop(); }); \
    auto btn = new QPushButton(tr("Set")); \
    btn->setFixedWidth(50); \
    btn->connect(btn, &QPushButton::pressed, btn, [this]{ set##Prop(); }); \
    auto group = LayoutV({label, LayoutH({edit, btn})}).makeGroupBox(title); \
    groups << group; \
    layout->addWidget(group); \
    lab##Prop = label; \
    ed##Prop = edit; \
    group##Prop = group; \
}

#define CHECK_PROP_STATUS(Prop, getStatus) \
    if (getStatus(hCam) != PEAK_ACCESS_READWRITE) { \
        lab##Prop->setText(tr("Not configurable")); \
        ed##Prop->setDisabled(true); \
    } else show##Prop();

#define PROP(Prop, setProp, getProp, getRange) \
    QLabel *lab##Prop; \
    CamPropEdit *ed##Prop; \
    QGroupBox *group##Prop; \
    \
    void show##Prop() { \
        auto edit = ed##Prop; \
        auto label = lab##Prop; \
        double value, min, max, step; \
        auto res = getProp(hCam, &value); \
        if (PEAK_ERROR(res)) { \
            label->setText(IDS.getPeakError(res)); \
            edit->setValue(0); \
            edit->setDisabled(true); \
            props[#Prop] = 0; \
            return; \
        } \
        edit->setValue(value); \
        edit->setDisabled(false); \
        props[#Prop] = value; \
        res = getRange(hCam, &min, &max, &step); \
        if (PEAK_ERROR(res)) \
            label->setText(IDS.getPeakError(res)); \
        else { \
            label->setText(QString("<b>Min = %1, Max = %2</b>").arg(min, 0, 'f', 2).arg(max, 0, 'f', 2)); \
            props[#Prop "Min"] = min; \
            props[#Prop "Max"] = max; \
            props[#Prop "Step"] = step; \
        }\
        if (ed##Prop == edExp) \
            showExpFreq(value); \
    } \
    void set##Prop() { \
        set##Prop##Raw(ed##Prop->value()); \
    } \
    bool set##Prop##Raw(double v) { \
        auto res = setProp(hCam, v); \
        if (PEAK_ERROR(res)) { \
            Ori::Dlg::error(IDS.getPeakError(res)); \
            return false; \
        } \
        if (ed##Prop == edFps || ed##Prop == edExp) { \
            showExp(); \
            showFps(); \
        } else show##Prop(); \
        return true; \
    } \
    void set##Prop##Fast(bool wheel, bool inc, bool big) { \
        double change = wheel \
            ? (big ? propChangeWheelBig : propChangeWheelSm) \
            : (big ? propChangeArrowBig : propChangeArrowSm); \
        double step = props[#Prop "Step"]; \
        double val = props[#Prop]; \
        double newVal; \
        if (inc) { \
            double max = props[#Prop "Max"]; \
            if (val >= max) return; \
            newVal = val * change; \
            if (newVal - val < step) newVal = val + step; \
            newVal = qMin(max, newVal); \
        } else { \
            double min = props[#Prop "Min"]; \
            if (val <= min) return; \
            newVal = val / change; \
            qDebug() << #Prop << "min=" << min << "step=" << step << "val=" << val; \
            if (val - newVal < step) newVal = val - step; \
            newVal = qMax(min, newVal); \
            qDebug() << #Prop << "set=" << newVal; \
        } \
        auto res = setProp(hCam, newVal); \
        if (PEAK_ERROR(res)) \
            Ori::Dlg::error(IDS.getPeakError(res)); \
        if (ed##Prop == edFps || ed##Prop == edExp) { \
            showExp(); \
            showFps(); \
        } else show##Prop(); \
    }

namespace {

peak_status setAnalogGain(peak_camera_handle hCam, double v) {
    return IDS.peak_Gain_Set(hCam, PEAK_GAIN_TYPE_ANALOG, PEAK_GAIN_CHANNEL_MASTER, v);
}
peak_status setDigitalGain(peak_camera_handle hCam, double v) {
    return IDS.peak_Gain_Set(hCam, PEAK_GAIN_TYPE_DIGITAL, PEAK_GAIN_CHANNEL_MASTER, v);
}
peak_status getAnalogGain(peak_camera_handle hCam, double *v) {
    return IDS.peak_Gain_Get(hCam, PEAK_GAIN_TYPE_ANALOG, PEAK_GAIN_CHANNEL_MASTER, v);
}
peak_status getDigitalGain(peak_camera_handle hCam, double *v) {
    return IDS.peak_Gain_Get(hCam, PEAK_GAIN_TYPE_DIGITAL, PEAK_GAIN_CHANNEL_MASTER, v);
}
peak_status getAnalogGainRange(peak_camera_handle hCam, double *min, double *max, double *inc) {
    return IDS.peak_Gain_GetRange(hCam, PEAK_GAIN_TYPE_ANALOG, PEAK_GAIN_CHANNEL_MASTER, min, max, inc);
}
peak_status getDigitalGainRange(peak_camera_handle hCam, double *min, double *max, double *inc) {
    return IDS.peak_Gain_GetRange(hCam, PEAK_GAIN_TYPE_DIGITAL, PEAK_GAIN_CHANNEL_MASTER, min, max, inc);
}
peak_access_status getAnalogGainAccessStatus(peak_camera_handle hCam) {
    return IDS.peak_Gain_GetAccessStatus(hCam, PEAK_GAIN_TYPE_ANALOG, PEAK_GAIN_CHANNEL_MASTER);
}
peak_access_status getDigitlGainAccessStatus(peak_camera_handle hCam) {
    return IDS.peak_Gain_GetAccessStatus(hCam, PEAK_GAIN_TYPE_DIGITAL, PEAK_GAIN_CHANNEL_MASTER);
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
        } else
            ValueEdit::keyPressEvent(e);
    }
    void wheelEvent(QWheelEvent *e) override {
        if (hasFocus()) {
            scrolled(true, e->angleDelta().y() > 0, e->modifiers().testFlag(Qt::ControlModifier));
            e->accept();
        } else
            ValueEdit::wheelEvent(e);
    }
};

} // namespace

//------------------------------------------------------------------------------
//                             IdsHardConfigPanelImpl
//------------------------------------------------------------------------------

class IdsHardConfigPanelImpl: public QWidget, public IAppSettingsListener
{
public:
    IdsHardConfigPanelImpl(peak_camera_handle hCam) : QWidget(), hCam(hCam)
    {
        auto layout = new QVBoxLayout(this);

        PROP_CONTROL(Exp, tr("Exposure (us)"))

        labExpFreq = new QLabel;
        groupExp->layout()->addWidget(labExpFreq);

        {
            auto label = new QLabel(tr("Percent of dynamic range:"));
            edAutoExp = new CamPropEdit;
            edAutoExp->connect(edAutoExp, &ValueEdit::keyPressed, edAutoExp, [this](int key){
                if (key == Qt::Key_Return || key == Qt::Key_Enter) autoExposure(); });
            butAutoExp = new QPushButton(tr("Find"));
            butAutoExp->setFixedWidth(50);
            connect(butAutoExp, &QPushButton::pressed, this, &IdsHardConfigPanelImpl::autoExposure);
            auto group = LayoutV({label, LayoutH({edAutoExp, butAutoExp})}).makeGroupBox(tr("Autoexposure"));
            groups << group;
            layout->addWidget(group);

            auto actnCopyLog = new QAction(tr("Copy Report"), this);
            connect(actnCopyLog, &QAction::triggered, this, &IdsHardConfigPanelImpl::copyAutoExpLog);
            butAutoExp->addAction(actnCopyLog);
            butAutoExp->setContextMenuPolicy(Qt::ActionsContextMenu);
        }

        PROP_CONTROL(Fps, tr("Frame rate"));
        PROP_CONTROL(AnalogGain, tr("Analog gain"))
        PROP_CONTROL(DigitalGain, tr("Digital gain"))

        CHECK_PROP_STATUS(Exp, IDS.peak_ExposureTime_GetAccessStatus)
        CHECK_PROP_STATUS(Fps, IDS.peak_FrameRate_GetAccessStatus)
        CHECK_PROP_STATUS(AnalogGain, getAnalogGainAccessStatus)
        CHECK_PROP_STATUS(DigitalGain, getDigitlGainAccessStatus)

        vertMirror = new QCheckBox(tr("Mirror vertically"));
        if (IDS.peak_Mirror_UpDown_GetAccessStatus(hCam) == PEAK_ACCESS_READWRITE) {
            vertMirror->setChecked(IDS.peak_Mirror_UpDown_IsEnabled(hCam));
        } else {
            vertMirror->setChecked(IDS.peak_IPL_Mirror_UpDown_IsEnabled(hCam));
            vertMirrorIPL = true;
        }
        connect(vertMirror, &QCheckBox::toggled, this, &IdsHardConfigPanelImpl::toggleVertMirror);
        layout->addWidget(vertMirror);

        horzMirror = new QCheckBox(tr("Mirror horizontally"));
        if (IDS.peak_Mirror_LeftRight_GetAccessStatus(hCam) == PEAK_ACCESS_READWRITE) {
            horzMirror->setChecked(IDS.peak_Mirror_LeftRight_IsEnabled(hCam));
        } else {
            horzMirror->setChecked(IDS.peak_IPL_Mirror_LeftRight_IsEnabled(hCam));
            horzMirrorIPL = true;
        }
        connect(horzMirror, &QCheckBox::toggled, this, &IdsHardConfigPanelImpl::toggleHorzMirror);
        layout->addWidget(horzMirror);

        // TODO: mirroring doesn't work, while works in IDS Cockpit for the same camera
        vertMirror->setVisible(false);
        horzMirror->setVisible(false);

        layout->addStretch();

        applySettings();
    }

    PROP(Exp, IDS.peak_ExposureTime_Set, IDS.peak_ExposureTime_Get, IDS.peak_ExposureTime_GetRange)
    PROP(Fps, IDS.peak_FrameRate_Set, IDS.peak_FrameRate_Get, IDS.peak_FrameRate_GetRange)
    PROP(AnalogGain, ::setAnalogGain, getAnalogGain, getAnalogGainRange)
    PROP(DigitalGain, ::setDigitalGain, getDigitalGain, getDigitalGainRange)

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

    void autoExposure()
    {
        if (props["Exp"] == 0) return;

        auto level = edAutoExp->value();
        if (level <= 0) level = 1;
        else if (level > 100) level = 100;
        edAutoExp->setValue(level);

        Ori::Settings s;
        s.beginGroup("DeviceControl");
        s.setValue("autoExposurePercent", level);

        autoExp = AutoExp();
        autoExp->hCam = hCam;
        autoExp->targetLevel = level / 100.0;
        autoExp->subStepMax = qMax(1, getCamProp(IdsHardConfigPanel::AUTOEXP_FRAMES_AVG).toInt());
        autoExp->getLevel = [this]{ watingBrightness = true; requestBrightness(this); };
        autoExp->showExpFps = [this](double exp, double fps){ edExp->setValue(exp); edFps->setValue(fps); };
        autoExp->finished = [this]{ stopAutoExp(); };

        for (auto group : groups)
            group->setDisabled(true);
        if (!autoExp->start())
            stopAutoExp();
    }

    void stopAutoExp()
    {
        watingBrightness = false;
        for (auto group : groups)
            group->setDisabled(false);
        double fps = props["Fps"];
        if (auto res = IDS.peak_FrameRate_Set(hCam, fps); PEAK_ERROR(res)) {
            qWarning() << LOG_ID << "Failed to restore FPS after autoexposure" << IDS.getPeakError(res);
        }
        showExp();
        showFps();
    }

    void copyAutoExpLog()
    {
        if (autoExp)
            qApp->clipboard()->setText(autoExp->logLines.join('\n'));
    }

    struct AutoExp
    {
        peak_camera_handle hCam;
        int step, subStep, subStepMax;
        double targetLevel, accLevel, fps;
        double exp, exp1, exp2, expMin, expMax, expStep;
        QStringList logLines;
        QString logLine;
        std::function<void()> getLevel;
        std::function<void()> finished;
        std::function<void(double, double)> showExpFps;

        class LogLine : public QTextStream
        {
        public:
            LogLine(QStringList &log, QString *line) : QTextStream(line), log(log), line(line) {}
            ~LogLine() { qDebug() << LOG_ID << "Autoexposure:" << (*line); log << (*line); }
            QStringList &log;
            QString *line;
        };

        LogLine log() {
            logLine = {};
            return LogLine(logLines, &logLine);
        }

        bool setExp(double v) {
            if (auto res = IDS.peak_ExposureTime_Set(hCam, v); PEAK_ERROR(res)) {
                QString msg = QString("Failed to set exposure to %1: %2").arg(v).arg(IDS.getPeakError(res));
                Ori::Dlg::error(msg);
                log() << msg;
                return false;
            }
            if (auto res = IDS.peak_ExposureTime_Get(hCam, &exp); PEAK_ERROR(res)) {
                QString msg = QString("Failed to get exposure after setting: %1").arg(IDS.getPeakError(res));
                Ori::Dlg::error(msg);
                log() << msg;
                return false;
            }
            if (auto res = IDS.peak_FrameRate_Get(hCam, &fps); PEAK_ERROR(res)) {
                QString msg = QString("Failed to get FPS after setting exposure: %1").arg(IDS.getPeakError(res));
                Ori::Dlg::error(msg);
                log() << msg;
                return false;
            }
            showExpFps(exp, fps);
            return true;
        }

        bool start()
        {
            log() << "target_level=" << targetLevel << " avg_frames=" << subStepMax;

            if (auto res = IDS.peak_ExposureTime_GetRange(hCam, &expMin, &expMax, &expStep); PEAK_ERROR(res)) {
                QString msg = QString("Failed to get exposure range: %1").arg(IDS.getPeakError(res));
                Ori::Dlg::error(msg);
                log() << msg;
                return false;
            }
            exp = expMin;
            if (auto res = IDS.peak_ExposureTime_Set(hCam, exp); PEAK_ERROR(res)) {
                QString msg = QString("Failed to set exposure to %1: %2").arg(exp).arg(IDS.getPeakError(res));
                Ori::Dlg::error(msg);
                log() << msg;
                return false;
            }
            double fpsMin, fpsMax, fpsInc;
            if (auto res = IDS.peak_FrameRate_GetRange(hCam, &fpsMin, &fpsMax, &fpsInc); PEAK_ERROR(res)) {
                QString msg = QString("Failed to get FPS range: %1").arg(IDS.getPeakError(res));
                Ori::Dlg::error(msg);
                log() << msg;
                return false;
            }
            fps = fpsMax;
            if (auto res = IDS.peak_FrameRate_Set(hCam, fps); PEAK_ERROR(res)) {
                QString msg = QString("Failed to set FPS to %1: %2").arg(fps).arg(IDS.getPeakError(res));
                Ori::Dlg::error(msg);
                log() << msg;
                return false;
            }
            exp1 = exp;
            exp2 = 0;
            step = 0;
            subStep = 0;
            accLevel = 0;
            getLevel();
            return true;
        }

        void doStep(double level)
        {
            accLevel += level;
            subStep++;

            log() << "step=" << step << " sub_step=" << subStep
                << " exp=" << exp << " fps=" << fps << " level=" << level
                << " avg_level=" << accLevel/double(subStep);

            if (subStep < subStepMax) {
                getLevel();
                return;
            }

            level = accLevel / double(subStep);

            if (qAbs(level - targetLevel) < 0.01) {
                log() << "stop_0=" << exp;
                goto stop;
            }

            if (level < targetLevel) {
                if (exp2 == 0) {
                    if (!setExp(qMin(exp1*2, expMax)))
                        goto stop;
                    // The above does not fail when setting higher-thah-max exposure
                    // It just clamps it to max and the loop never ends.
                    // So need an explicit check:
                    if (exp >= expMax) {
                        log() << "underexposed=" << exp;
                        Ori::Dlg::warning(tr("Underexposed"));
                        goto stop;
                    }
                    exp1 = exp;
                } else {
                    exp1 = exp;
                    if (!setExp((exp1+exp2)/2.0))
                        goto stop;
                    if (qAbs(exp1 - exp) <= expStep) {
                        log() << "stop_1 " << exp;
                        goto stop;
                    }
                }
            } else {
                if (exp2 == 0) {
                    if (exp == expMin) {
                        log() << "overexposed=" << exp;
                        Ori::Dlg::warning(tr("Overexposed"));
                        goto stop;
                    }
                    exp2 = exp1;
                    exp1 /= 2.0;
                    if (!setExp((exp1+exp2)/2.0))
                        goto stop;
                } else {
                    exp2 = exp;
                    if (!setExp((exp1+exp2)/2.0))
                        goto stop;
                    if (qAbs(exp2 - exp) <= expStep) {
                        log() << "stop_2=" << exp;
                        goto stop;
                    }
                }
            }
            step++;
            subStep = 0;
            accLevel = 0;
            getLevel();
            return;

        stop:
            finished();
        }
    };

    void applySettings()
    {
        auto &s = AppSettings::instance();
        propChangeWheelSm = 1 + double(s.propChangeWheelSm) / 100.0;
        propChangeWheelBig = 1 + double(s.propChangeWheelBig) / 100.0;
        propChangeArrowSm = 1 + double(s.propChangeArrowSm) / 100.0;
        propChangeArrowBig = 1 + double(s.propChangeArrowBig) / 100.0;

        Ori::Settings s1;
        s1.beginGroup("DeviceControl");
        edAutoExp->setValue(s1.value("autoExposurePercent", 80).toInt());
    }

    void settingsChanged() override
    {
        applySettings();
    }

    void toggleVertMirror(bool on)
    {
        auto res = vertMirrorIPL
            ? IDS.peak_IPL_Mirror_UpDown_Enable(hCam, on)
            : IDS.peak_Mirror_UpDown_Enable(hCam, on);
        if (PEAK_ERROR(res))
            Ori::Dlg::error(IDS.getPeakError(res));
        qDebug() << IDS.peak_IPL_Mirror_UpDown_IsEnabled(hCam);
    }

    void toggleHorzMirror(bool on)
    {
        auto res = horzMirrorIPL
            ? IDS.peak_IPL_Mirror_LeftRight_Enable(hCam, on)
            : IDS.peak_Mirror_LeftRight_Enable(hCam, on);
        if (PEAK_ERROR(res))
            Ori::Dlg::error(IDS.getPeakError(res));
    }

    peak_camera_handle hCam;
    QList<QGroupBox*> groups;
    CamPropEdit *edAutoExp;
    QPushButton *butAutoExp;
    QLabel *labExpFreq;
    QCheckBox *vertMirror, *horzMirror;
    bool vertMirrorIPL = false, horzMirrorIPL = false;
    QMap<const char*, double> props;
    double propChangeWheelSm, propChangeWheelBig;
    double propChangeArrowSm, propChangeArrowBig;
    bool watingBrightness = false;
    bool closeRequested = false;
    std::function<void(QObject*)> requestBrightness;
    std::function<QVariant(IdsHardConfigPanel::CamProp)> getCamProp;
    std::optional<AutoExp> autoExp;

protected:
    void closeEvent(QCloseEvent *e) override
    {
        QWidget::closeEvent(e);
        // Event loop will crash if there is no event receiver anymore
        // Wait for the next brightness event and close after that
        if (watingBrightness) {
            closeRequested = true;
            e->ignore();
        }
    }

    bool event(QEvent *event) override
    {
        if (auto e = dynamic_cast<BrightEvent*>(event); e) {
            watingBrightness = false;
            if (closeRequested)
                close();
            else if (autoExp)
                autoExp->doStep(e->level);
            return true;
        }
        return QWidget::event(event);
    }
};

//------------------------------------------------------------------------------
//                              IdsHardConfigPanel
//-----------------------------------------------------------------------------

IdsHardConfigPanel::IdsHardConfigPanel(peak_camera_handle hCam,
    std::function<void(QObject*)> requestBrightness,
    std::function<QVariant(CamProp)> getCamProp,
    QWidget *parent) : HardConfigPanel(parent)
{
    _impl = new IdsHardConfigPanelImpl(hCam);
    _impl->requestBrightness = requestBrightness;
    _impl->getCamProp = getCamProp;

    auto scroll = new QScrollArea;
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(_impl);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll);
}

void IdsHardConfigPanel::setReadOnly(bool on)
{
    for (auto group : _impl->groups)
        group->setDisabled(on);
}

#endif // WITH_IDS
