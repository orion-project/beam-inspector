#include "IdsHardConfig.h"

#ifdef WITH_IDS

#include "app/AppSettings.h"
#include "app/HelpSystem.h"
#include "cameras/CameraTypes.h"
#include "cameras/IdsLib.h"

#include "core/OriFloatingPoint.h"
#include "helpers/OriLayouts.h"
#include "helpers/OriDialogs.h"
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

// Exposure preset field names
#define PKEY_NAME "name"
#define PKEY_EXP "exposure"
#define PKEY_AEXP "auto_exposure"
#define PKEY_FPS "frame_rate"
#define PKEY_AGAIN "analog_gain"
#define PKEY_DGAIN "digital_gain"

using namespace Ori::Layouts;
using namespace Ori::Widgets;

bool theSame(const AnyRecord &p1, const AnyRecord &p2)
{
    for (auto it = p1.constBegin(); it != p1.constEnd(); it++) {
        if (it.key() == QStringLiteral("name"))
            continue;
        bool ok;
        auto v1 = it.value().toDouble(&ok);
        if (!ok)
            return false;
        auto v2 = p2.value(it.key()).toDouble(&ok);
        if (!ok)
            return false;
        // Humans should see the same text, if they don't then values are different
        if (QString::number(v1) != QString::number(v2)) {
            //qDebug() << "not same numbers" << v1 << v2 << QString::number(v1) << QString::number(v2);
            return false;
        }
    }
    return true;
}

#define PROP_CONTROL(Prop, title) { \
    auto label = new QLabel; \
    label->setWordWrap(true); \
    label->setForegroundRole(qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark ? QPalette::Light : QPalette::Mid); \
    auto edit = new CamPropEdit; \
    edit->scrolled = [this](bool wheel, bool inc, bool big){ set##Prop##Fast(wheel, inc, big); }; \
    edit->connect(edit, &ValueEdit::focused, edit, [this](bool focus){ if (!focus) show##Prop(); }); \
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
        if (ed##Prop == edFps && roundFps) { \
            /* Sometimes camera returns not exactly the same that was set
             * e.g. 14.9988 instead of 15, or 9.9984 instead of 10
             * We are not interested in such small differences
            */ \
            value = qRound(value * 100.0) / 100.0; \
        } else if (ed##Prop == edExp && roundExp) { \
            value = qRound(value / 10.0) * 10; \
        } \
        /*qDebug() << "showProp" << #Prop << value;*/ \
        edit->setValue(value); \
        edit->setDisabled(false); \
        props[#Prop] = value; \
        res = getRange(hCam, &min, &max, &step); \
        if (PEAK_ERROR(res)) \
            label->setText(IDS.getPeakError(res)); \
        else { \
            if (ed##Prop == edFps && roundFps) { \
                min = qRound(min * 100.0) / 100.0; \
                max = qRound(max * 100.0) / 100.0; \
                label->setText(QString("<b>Min = %1, Max = %2</b>").arg(min, 0, 'f', 2).arg(max, 0, 'f', 2)); \
            } else if (ed##Prop == edExp && roundExp) { \
                min = qRound(min / 10.0) * 10; \
                max = qRound(max / 10.0) * 10; \
                label->setText(QString("<b>Min = %1, Max = %2</b>").arg(min, 0, 'f', 0).arg(max, 0, 'f', 0)); \
            } else { \
                label->setText(QString("<b>Min = %1, Max = %2</b>").arg(min, 0, 'f', 2).arg(max, 0, 'f', 2)); \
            }\
            props[#Prop "Min"] = min; \
            props[#Prop "Max"] = max; \
            props[#Prop "Step"] = step; \
        }\
        if (ed##Prop == edExp) \
            showExpFreq(value); \
        if (!bulkSetProps) \
            highightPreset();\
    } \
    double get##Prop##Raw() const { \
        double value = 0; \
        auto res = getProp(hCam, &value); \
        if (PEAK_ERROR(res)) \
            qWarning() << LOG_ID << "getPropRaw" << #Prop << IDS.getPeakError(res); \
        return value; \
    } \
    void set##Prop() { \
        double oldValue = props[#Prop]; \
        double newValue = ed##Prop->value(); \
        if (Double(oldValue).almostEqual(Double(newValue))) \
            return; \
        set##Prop##Raw(newValue, true); \
        if (ed##Prop != edFps) \
            exposureChanged(); \
    } \
    bool set##Prop##Raw(double v, bool showErr) { \
        auto res = setProp(hCam, v); \
        if (PEAK_ERROR(res)) { \
            if (showErr) \
                Ori::Dlg::error(IDS.getPeakError(res)); \
            else \
                qWarning() << LOG_ID << "setPropRaw" << #Prop << IDS.getPeakError(res); \
            return false; \
        } \
        /*qDebug() << "setPropRaw" << #Prop << v;*/ \
        if (ed##Prop == edFps) { \
            showExp(); \
            showFps(); \
            if (fpsLock > 0) { \
                fpsLock = v; \
                showFpsLock(); \
            } \
        } else if (ed##Prop == edExp) { \
            showExp(); \
            showFps(); \
            if (fpsLock > 0) { \
                showFpsLock(); \
                setFpsRaw(fpsLock, false); \
            } \
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
            if (val - newVal < step) newVal = val - step; \
            newVal = qMax(min, newVal); \
        } \
        auto res = setProp(hCam, newVal); \
        if (PEAK_ERROR(res)) \
            Ori::Dlg::error(IDS.getPeakError(res)); \
        if (ed##Prop == edFps) { \
            showExp(); \
            showFps(); \
            if (fpsLock > 0) { \
                fpsLock = newVal; \
                showFpsLock(); \
            } \
        } else if (ed##Prop == edExp) { \
            showExp(); \
            showFps(); \
            if (fpsLock > 0) { \
                showFpsLock(); \
                setFpsRaw(fpsLock, false); \
            } \
        } else show##Prop(); \
        if (ed##Prop != edFps) \
            exposureChanged(); \
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

        groupPresets = LayoutV({}).makeGroupBox(tr("Presets"));
        layout->addWidget(groupPresets);

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

        butFpsLock = new QPushButton(tr("Lock frame rate"));
        butFpsLock->setCheckable(true);
        connect(butFpsLock, &QPushButton::clicked, this, &IdsHardConfigPanelImpl::toggleFpsLock);
        groupFps->layout()->addWidget(butFpsLock);

        PROP_CONTROL(AnalogGain, tr("Analog gain"))
        PROP_CONTROL(DigitalGain, tr("Digital gain"))

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
    }

    PROP(Exp, IDS.peak_ExposureTime_Set, IDS.peak_ExposureTime_Get, IDS.peak_ExposureTime_GetRange)
    PROP(Fps, IDS.peak_FrameRate_Set, IDS.peak_FrameRate_Get, IDS.peak_FrameRate_GetRange)
    PROP(AnalogGain, ::setAnalogGain, getAnalogGain, getAnalogGainRange)
    PROP(DigitalGain, ::setDigitalGain, getDigitalGain, getDigitalGainRange)

    void showInitialValues()
    {
        bulkSetProps = true;

        CHECK_PROP_STATUS(Exp, IDS.peak_ExposureTime_GetAccessStatus)
        CHECK_PROP_STATUS(Fps, IDS.peak_FrameRate_GetAccessStatus)
        CHECK_PROP_STATUS(AnalogGain, getAnalogGainAccessStatus)
        CHECK_PROP_STATUS(DigitalGain, getDigitlGainAccessStatus)

        auto level = getCamProp(IdsHardConfigPanel::AUTOEXP_LEVEL).toInt();
        if (level == 0) level = 80;
        edAutoExp->setValue(level);

        fpsLock = getCamProp(IdsHardConfigPanel::FPS_LOCK).toDouble();
        showFpsLock();

        bulkSetProps = false;
    }

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

        setCamProp(IdsHardConfigPanel::AUTOEXP_LEVEL, level);

        autoExp = AutoExp();
        autoExp->hCam = hCam;
        autoExp->targetLevel = level / 100.0;
        autoExp->subStepMax = qMax(1, getCamProp(IdsHardConfigPanel::AUTOEXP_FRAMES_AVG).toInt());
        autoExp->getLevel = [this]{ watingBrightness = true; requestBrightness(this); };
        autoExp->showExpFps = [this](double exp, double fps){
            props["Exp"] = exp;
            props["Fps"] = fps;
            edExp->setValue(exp);
            edFps->setValue(fps);
            if (fpsLock > 0)
                showFpsLock();
        };
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
        exposureChanged();
        if (fpsLock > 0) {
            showFpsLock();
            setFpsRaw(fpsLock, false);
        }
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

    void applySettings(bool onChange)
    {
        auto &s = AppSettings::instance();
        propChangeWheelSm = 1 + double(s.propChangeWheelSm) / 100.0;
        propChangeWheelBig = 1 + double(s.propChangeWheelBig) / 100.0;
        propChangeArrowSm = 1 + double(s.propChangeArrowSm) / 100.0;
        propChangeArrowBig = 1 + double(s.propChangeArrowBig) / 100.0;

        if (roundFps != s.roundHardConfigFps) {
            roundFps = s.roundHardConfigFps;
            if (onChange) showFps();
        }
        if (roundExp != s.roundHardConfigExp) {
           roundExp = s.roundHardConfigExp;
            if (onChange) showExp();
        }
    }

    // IAppSettingsListener
    void settingsChanged() override
    {
        applySettings(true);
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

    inline AnyRecords* getPresets()
    {
        return getCamProp(IdsHardConfigPanel::EXP_PRESETS).value<AnyRecords*>();
    }

    inline AnyRecord makePreset()
    {
        return {
            { PKEY_EXP, props["Exp"] },
            { PKEY_AEXP, edAutoExp->value() },
            { PKEY_FPS, props["Fps"] },
            { PKEY_AGAIN, props["AnalogGain"] },
            { PKEY_DGAIN, props["DigitalGain"] },
        };
    }

    void saveNewPreset()
    {
        auto preset = makePreset();
        Ori::Dlg::InputTextOptions opts;
        opts.label = formatPreset(preset) + tr("<p>Enter preset name:</p>");
        opts.onHelp = []{ HelpSystem::topic("exp_presets"); };
        if (Ori::Dlg::inputText(opts)) {
            preset[PKEY_NAME] = opts.value;
            auto presets = getPresets();
            presets->append(preset);
            setCamProp(IdsHardConfigPanel::EXP_PRESETS, {});
            QApplication::postEvent(this, new UpdateSettingsEvent());
        }
    }

    QString formatPreset(const AnyRecord &preset)
    {
        QStringList s;
        s << QStringLiteral("<code>");
        // Trailing spaces are right margin for better look in the info window
        s << tr("Exposure(us):  <b>%1</b>    <br>").arg(preset[PKEY_EXP].toDouble());
        s << tr("Autoexposure:  <b>%1%</b><br>").arg(preset[PKEY_AEXP].toInt());
        s << tr("Frame rate:    <b>%1</b><br>").arg(preset[PKEY_FPS].toDouble());
        s << tr("Analog gain:   <b>%1</b><br>").arg(preset[PKEY_AGAIN].toDouble());
        s << tr("Digital gain:  <b>%1</b><br>").arg(preset[PKEY_DGAIN].toDouble());
        s << QStringLiteral("</code>");
        QString r = s.join(QString());
        // QLabel collapses spaces even in code block
        r.replace(' ', QStringLiteral("&nbsp;"));
        return r;
    }

    void highightPreset()
    {
        if (!getCamProp)
            return;
        for (auto b : presetButtons)
            b->setChecked(false);
        auto props = makePreset();
        auto presets = getPresets();
        for (int i = 0; i < presets->size(); i++) {
            auto const &preset = presets->at(i);
            if (theSame(props, preset)) {
                if (i < presetButtons.size()) {
                    presetButtons.at(i)->setChecked(true);
                    return;
                }
            }
        }
    }

    void makePresetButtons()
    {
        auto presets = getPresets();
        QMap<QString, QPushButton*> buttons;
        for (auto b : presetButtons) {
            groupPresets->layout()->removeWidget(b);
        }
        qDeleteAll(presetButtons);
        presetButtons.clear();
        for (int i = 0; i < presets->size(); i++) {
            const auto &preset = presets->at(i);

            QList<QAction*> actions;

            auto b = new QPushButton(preset["name"].toString());
            b->setCheckable(true);
            b->setContextMenuPolicy(Qt::ActionsContextMenu);
            connect(b, &QPushButton::clicked, this, &IdsHardConfigPanelImpl::applyPreset);
            groupPresets->layout()->addWidget(b);
            presetButtons << b;

            auto actInfo = new QAction(QIcon(":/toolbar/info"), tr("Preset info..."), b);
            connect(actInfo, &QAction::triggered, this, &IdsHardConfigPanelImpl::showPresetInfo);
            actions << actInfo;

            auto actRename = new QAction(tr("Rename preset..."), b);
            connect(actRename, &QAction::triggered, this, &IdsHardConfigPanelImpl::renamePreset);
            actions << actRename;

            auto actUpdate = new QAction(tr("Save current values to preset..."), b);
            connect(actUpdate, &QAction::triggered, this, &IdsHardConfigPanelImpl::updatePreset);
            actions << actUpdate;

            auto actSep = new QAction(b);
            actSep->setSeparator(true);
            actions << actSep;

            auto actDel = new QAction(QIcon(":/toolbar/trash"), tr("Remove preset..."), b);
            connect(actDel, &QAction::triggered, this, &IdsHardConfigPanelImpl::removePreset);
            actions << actDel;

            for (auto a : actions) a->setData(i);
            b->addActions(actions);
        }
        auto b = new QPushButton(tr(" Add new preset..."));
        b->setIcon(QIcon(":/toolbar/plus"));
        connect(b, &QPushButton::clicked, this, &IdsHardConfigPanelImpl::saveNewPreset);
        groupPresets->layout()->addWidget(b);
        presetButtons << b;
        adjustSize();
        highightPreset();
    }

    void remakePresetButtons()
    {
        QApplication::postEvent(this, new UpdateSettingsEvent());
    }

    void applyPreset()
    {
        PresetsHandler(this, presetButtons.indexOf(qobject_cast<QPushButton*>(sender()))).apply();
    }

    void showPresetInfo()
    {
        PresetsHandler(this, qobject_cast<QAction*>(sender())->data().toInt()).info();
    }

    void renamePreset()
    {
        PresetsHandler(this, qobject_cast<QAction*>(sender())->data().toInt()).rename();
    }

    void updatePreset()
    {
        PresetsHandler(this, qobject_cast<QAction*>(sender())->data().toInt()).update();
    }

    void removePreset()
    {
        PresetsHandler(this, qobject_cast<QAction*>(sender())->data().toInt()).remove();
    }

    struct PresetsHandler
    {
        AnyRecords *presets;
        AnyRecord *preset = nullptr;
        IdsHardConfigPanelImpl *parent;
        int index;

        PresetsHandler(IdsHardConfigPanelImpl *parent, int index) : parent(parent), index(index)
        {
            presets = parent->getPresets();
            if (index < 0 || index >= presets->size()) {
                qWarning() << "Preset index out of range:" << index << "| Presets count:" << presets->size();
                return;
            }
            preset = &((*presets)[index]);
        }

        void apply()
        {
            if (!preset) return;
            bool ok;
            parent->bulkSetProps = true;
            QStringList failed;
            if (auto v = (*preset)[PKEY_DGAIN].toDouble(&ok); ok) {
                if (!parent->setDigitalGainRaw(v, false)) {
                    failed << PKEY_DGAIN;
                    qDebug() << LOG_ID << "Preset not fully applied:" << PKEY_DGAIN
                        << "| preset =" << v << "| camera =" << parent->getDigitalGainRaw();
                }
            }
            if (auto v = (*preset)[PKEY_AGAIN].toDouble(&ok); ok) {
                if (!parent->setAnalogGainRaw(v, false)) {
                    failed << PKEY_AGAIN;
                    qDebug() << LOG_ID << "Preset not fully applied:" << PKEY_AGAIN
                        << "| preset =" << v << "| camera =" << parent->getAnalogGainRaw();
                }
            }
            if (auto v = (*preset)[PKEY_EXP].toDouble(&ok); ok) {
                if (!parent->setExpRaw(v, false)) {
                    failed << PKEY_EXP;
                    qDebug() << LOG_ID << "Preset not fully applied:" << PKEY_EXP
                        << "| preset =" << v << "| camera =" << parent->getExpRaw();
                }
            }
            if (auto v = (*preset)[PKEY_FPS].toDouble(&ok); ok) {
                if (!parent->setFpsRaw(v, false)) {
                    failed << PKEY_FPS;
                    qDebug() << LOG_ID << "Preset not fully applied:" << PKEY_FPS
                        << "| preset =" << v << "| camera =" << parent->getFpsRaw();
                }
            }
            if (auto v = (*preset)[PKEY_AEXP].toInt(&ok); ok) {
                parent->edAutoExp->setValue(v);
                parent->setCamProp(IdsHardConfigPanel::AUTOEXP_LEVEL, v);
            }
            parent->bulkSetProps = false;
            parent->highightPreset();
            parent->exposureChanged();
            if (!failed.empty())
                Ori::Dlg::warning(tr("Preset are not fully applied. Failed to set some properties: %1").arg(failed.join(", ")));
        }

        void info()
        {
            if (!preset) return;
            Ori::Dlg::info(parent->formatPreset(*preset));
        }

        void rename()
        {
            if (!preset) return;
            Ori::Dlg::InputTextOptions opts;
            opts.value = (*preset)[PKEY_NAME].toString();
            opts.label = parent->formatPreset(*preset) + tr("<p>A new name for preset:</p>");
            opts.onHelp = []{ HelpSystem::topic("exp_presets"); };
            if (Ori::Dlg::inputText(opts)) {
                (*preset)[PKEY_NAME] = opts.value;
                parent->setCamProp(IdsHardConfigPanel::EXP_PRESETS, {});
                parent->presetButtons.at(index)->setText(opts.value);
            }
        }

        void update()
        {
            if (!preset) return;
            auto newPreset = parent->makePreset();
            auto confirm = parent->formatPreset(newPreset) +
                tr("<p>Update <b>%1</b> with these values?</p>").arg((*preset)[PKEY_NAME].toString());
            if (Ori::Dlg::yes(confirm)) {
                (*presets)[index] = newPreset;
                parent->setCamProp(IdsHardConfigPanel::EXP_PRESETS, {});
                parent->highightPreset();
            }
        }

        void remove()
        {
            if (!preset) return;
            if (Ori::Dlg::yes(tr("The preset will be removed:<br><br><b>%1</b>").arg((*preset)[PKEY_NAME].toString()))) {
                presets->removeAt(index);
                parent->setCamProp(IdsHardConfigPanel::EXP_PRESETS, {});
                parent->remakePresetButtons();
            }
        }
    };

    void showFpsLock()
    {
        bool locked = fpsLock > 0;
        bool lockOk = (int)qRound(fpsLock * 100) == (int)qRound(props["Fps"] * 100);
        butFpsLock->setChecked(locked);
        butFpsLock->setText(locked ? tr(" Target: %1 FPS").arg(fpsLock) : tr(" Lock frame rate"));
        butFpsLock->setIcon(QIcon(!locked ? ":/toolbar/lock_on" : (lockOk ? ":/toolbar/ok" : ":/toolbar/exclame")));
        if (locked && !lockOk)
            butFpsLock->setToolTip(tr("Target value is out of available range"));
        else butFpsLock->setToolTip({});
    }

    void toggleFpsLock(bool checked)
    {
        fpsLock = checked ? props["Fps"] : 0;
        setCamProp(IdsHardConfigPanel::FPS_LOCK, fpsLock);
        showFpsLock();
    }

    peak_camera_handle hCam;
    QList<QGroupBox*> groups;
    CamPropEdit *edAutoExp;
    QPushButton *butAutoExp;
    QLabel *labExpFreq;
    QGroupBox *groupPresets;
    QList<QPushButton*> presetButtons;
    QCheckBox *vertMirror, *horzMirror;
    bool vertMirrorIPL = false, horzMirrorIPL = false;
    QMap<const char*, double> props;
    double propChangeWheelSm, propChangeWheelBig;
    double propChangeArrowSm, propChangeArrowBig;
    bool watingBrightness = false;
    bool closeRequested = false;
    bool bulkSetProps = false;
    bool roundFps = true;
    bool roundExp = true;
    double fpsLock = 0;
    QPushButton *butFpsLock;
    std::function<void(QObject*)> requestBrightness;
    std::function<QVariant(IdsHardConfigPanel::CamProp)> getCamProp;
    std::function<void(IdsHardConfigPanel::CamProp, QVariant)> setCamProp;
    std::function<void()> exposureChanged;
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
        if (dynamic_cast<UpdateSettingsEvent*>(event)) {
            makePresetButtons();
            return true;
        }
        return QWidget::event(event);
    }
};

//------------------------------------------------------------------------------
//                              IdsHardConfigPanel
//-----------------------------------------------------------------------------

IdsHardConfigPanel::IdsHardConfigPanel(peak_camera_handle hCam,
    std::function<QVariant(CamProp)> getCamProp,
    std::function<void(CamProp, QVariant)> setCamProp,
    std::function<void(QObject*)> requestBrightness,
    std::function<void()> exposureChanged,
    QWidget *parent) : HardConfigPanel(parent)
{
    _impl = new IdsHardConfigPanelImpl(hCam);
    _impl->getCamProp = getCamProp;
    _impl->setCamProp = setCamProp;
    _impl->exposureChanged = exposureChanged;
    _impl->requestBrightness = requestBrightness;
    _impl->applySettings(false);
    _impl->makePresetButtons();
    _impl->showInitialValues();
    _impl->highightPreset();

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

bool IdsHardConfigPanel::event(QEvent *event)
{
    if (dynamic_cast<UpdateSettingsEvent*>(event)) {
        _impl->makePresetButtons();
        return true;
    }
    return QWidget::event(event);
}
#endif // WITH_IDS
