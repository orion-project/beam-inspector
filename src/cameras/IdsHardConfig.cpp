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
            butAutoExp->connect(butAutoExp, &QPushButton::pressed, butAutoExp, [this]{ autoExposure(); });
            auto group = LayoutV({label, LayoutH({edAutoExp, butAutoExp})}).makeGroupBox(tr("Autoexposure"));
            groups << group;
            layout->addWidget(group);
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

        butAutoExp->setDisabled(true);
        edAutoExp->setDisabled(true);

        auto level = edAutoExp->value();
        if (level <= 0) level = 1;
        else if (level > 100) level = 100;
        edAutoExp->setValue(level);
        autoExpLevel = level / 100.0;
        qDebug() << LOG_ID << "Autoexposure" << autoExpLevel;

        Ori::Settings s;
        s.beginGroup("DeviceControl");
        s.setValue("autoExposurePercent", level);

        if (!setExpRaw(props["ExpMin"]))
            return;
        autoExp1 = props["Exp"];
        autoExp2 = 0;
        autoExpStep = 0;
        watingBrightness = true;
        requestBrightness(this);
    }

    void autoExposureStep(double level)
    {
        qDebug() << LOG_ID << "Autoexposure step" << autoExpStep << "| exp" << props["Exp"] << "| level" << level;

        if (qAbs(level - autoExpLevel) < 0.01) {
            qDebug() << LOG_ID << "Autoexposure: stop(0)" << props["Exp"];
            goto stop;
        }

        if (level < autoExpLevel) {
            if (autoExp2 == 0) {
                if (!setExpRaw(qMin(autoExp1*2, props["ExpMax"])))
                    goto stop;
                // The above does not fail when setting higher-thah-max exposure
                // It just clamps it to max and the loop never ends.
                // So need an explicit check:
                if (props["Exp"] >= props["ExpMax"]) {
                    qDebug() << LOG_ID << "Autoexposure: underexposed" << props["Exp"];
                    Ori::Dlg::warning(tr("Underexposed"));
                    goto stop;
                }
                autoExp1 = props["Exp"];
            } else {
                autoExp1 = props["Exp"];
                if (!setExpRaw((autoExp1+autoExp2)/2))
                    goto stop;
                if (qAbs(autoExp1 - props["Exp"]) <= props["ExpStep"]) {
                    qDebug() << LOG_ID << "Autoexposure: stop(1)" << props["Exp"];
                    goto stop;
                }
            }
        } else {
            if (autoExp2 == 0) {
                if (props["Exp"] == props["ExpMin"]) {
                    qDebug() << LOG_ID << "Autoexposure: Overexposed";
                    Ori::Dlg::warning(tr("Overexposed"));
                    goto stop;
                }
                autoExp2 = autoExp1;
                autoExp1 = autoExp2/2;
                if (!setExpRaw((autoExp1+autoExp2)/2))
                    goto stop;
            } else {
                autoExp2 = props["Exp"];
                if (!setExpRaw((autoExp1+autoExp2)/2))
                    goto stop;
                if (qAbs(autoExp2 - props["Exp"]) <= props["ExpStep"]) {
                    qDebug() << LOG_ID << "Autoexposure: stop(2)" << props["Exp"];
                    goto stop;
                }
            }
        }
        autoExpStep++;
        watingBrightness = true;
        requestBrightness(this);
        return;

    stop:
        butAutoExp->setDisabled(false);
        edAutoExp->setDisabled(false);
    }

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
    CamPropEdit *edAutoExp;
    QList<QGroupBox*> groups;
    QPushButton *butAutoExp;
    QLabel *labExpFreq;
    QCheckBox *vertMirror, *horzMirror;
    bool vertMirrorIPL = false, horzMirrorIPL = false;
    QMap<const char*, double> props;
    double propChangeWheelSm, propChangeWheelBig;
    double propChangeArrowSm, propChangeArrowBig;
    double autoExpLevel, autoExp1, autoExp2;
    bool watingBrightness = false;
    bool closeRequested = false;
    int autoExpStep;
    std::function<void(QObject*)> requestBrightness;

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
            else
                autoExposureStep(e->level);
            return true;
        }
        return QWidget::event(event);
    }
};

//------------------------------------------------------------------------------
//                              IdsHardConfigPanel
//-----------------------------------------------------------------------------

IdsHardConfigPanel::IdsHardConfigPanel(peak_camera_handle hCam, std::function<void(QObject*)> requestBrightness, QWidget *parent) : HardConfigPanel(parent)
{
    _impl = new IdsHardConfigPanelImpl(hCam);
    _impl->requestBrightness = requestBrightness;

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
