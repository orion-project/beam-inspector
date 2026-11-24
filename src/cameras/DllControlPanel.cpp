#include "DllControlPanel.h"

#include "app/AppSettings.h"
#include "cameras/AutoExposure.h"
#include "widgets/PlotHelpers.h"

#include "core/OriFloatingPoint.h"
#include "helpers/OriDialogs.h"
#include "helpers/OriLayouts.h"
#include "widgets/OriValueEdit.h"

#include <QAction>
#include <QApplication>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QWheelEvent>

#define LOG_ID "DllControlPanel:"

using namespace Ori::Layouts;
using namespace Ori::Widgets;

namespace {

enum class CamPropId { Expo, Fps, AGain, DGain };

class CamPropEdit : public ValueEdit
{
public:
    std::function<void(bool, bool, bool)> scrolled;
protected:
    void keyPressEvent(QKeyEvent *e) override {
        if (e->key() == Qt::Key_Up && scrolled) {
            scrolled(false, true, e->modifiers().testFlag(Qt::ControlModifier));
            e->accept();
        } else if (e->key() == Qt::Key_Down && scrolled) {
            scrolled(false, false, e->modifiers().testFlag(Qt::ControlModifier));
            e->accept();
        } else
            ValueEdit::keyPressEvent(e);
    }
    void wheelEvent(QWheelEvent *e) override {
        if (hasFocus() && scrolled) {
            scrolled(true, e->angleDelta().y() > 0, e->modifiers().testFlag(Qt::ControlModifier));
            e->accept();
        } else
            ValueEdit::wheelEvent(e);
    }
};

struct CamPropSettings
{
    double changeWheelSm, changeWheelBig;
    double changeArrowSm, changeArrowBig;
    bool roundFps = true;
    bool roundExpo = true;
};

struct CamPropControl
{
    CamPropId id;
    QString title;
    QLabel *label;
    QLabel *auxLabel = nullptr;
    CamPropEdit *edit;
    QGroupBox *groupBox;
    double curValue = 0;
    double minValue = 0, maxValue = 0, valueStep = 0;
    CamPropSettings *settings;
    std::function<IBeamStatus*(double*, double*, double*)> getRange;
    std::function<IBeamStatus*(double*)> getValue;
    std::function<IBeamStatus*(double)> setValue;
    std::function<void()> onChanged;
    
    void makeWidgets(QVBoxLayout* layout)
    {
        label = new QLabel;
        label->setWordWrap(true);
        label->setForegroundRole(PlotHelpers::isDarkTheme() ? QPalette::Light : QPalette::Mid);
    
        edit = new CamPropEdit; \
        edit->scrolled = [this](bool wheel, bool inc, bool big){
            applyValueFast(wheel, inc, big);
        };
        edit->connect(edit, &ValueEdit::focused, edit, [this](bool focus){
            if (!focus) showValue();
        });
        edit->connect(edit, &ValueEdit::keyPressed, edit, [this](int key){
            if (key == Qt::Key_Return || key == Qt::Key_Enter) applyValue();
        });
        
        auto btn = new QPushButton(qApp->tr("Set", "DllControlPanel"));
        btn->setFixedWidth(50); \
        btn->connect(btn, &QPushButton::pressed, btn, [this]{ applyValue(); });
        
        groupBox = LayoutV({label, LayoutH({edit, btn})}).makeGroupBox(title);
        
        if (id == CamPropId::Expo) {
            auxLabel = new QLabel;
            groupBox->layout()->addWidget(auxLabel);
        }
        
        layout->addWidget(groupBox);
    }
    
    void showValue()
    {
        double value;
        auto res = getValue(&value);
        if (res) {
            label->setText(DllAdapter::errorText(res));
            edit->setValue(0);
            edit->setDisabled(true);
            curValue = 0;
            if (auxLabel)
                auxLabel->clear();
            return;
        }
        // Sometimes camera returns not exactly the same that was set
        // e.g. 14.9988 instead of 15, or 9.9984 instead of 10
        // We are not interested in such small differences
        if (id == CamPropId::Fps && settings->roundFps) {
            value = qRound(value * 100.0) / 100.0;
        } else if (id == CamPropId::Expo && settings->roundExpo) {
            value = qRound(value / 10.0) * 10;
        }
        edit->setValue(value);
        edit->setDisabled(false);
        curValue = value;
        
        double min, max, step;
        res = getRange(&min, &max, &step);
        if (res)
            label->setText(DllAdapter::errorText(res));
        else {
            if (id == CamPropId::Fps && settings->roundFps) {
                min = qRound(min * 100.0) / 100.0;
                max = qRound(max * 100.0) / 100.0;
            } else if (id == CamPropId::Expo && settings->roundExpo) {
                min = qRound(min / 10.0) * 10;
                max = qRound(max / 10.0) * 10;
            }
            label->setText(QString("<b>Min = %1, Max = %2</b>").arg(min, 0, 'f', 2).arg(max, 0, 'f', 2));
            minValue = min;
            maxValue = max;
            valueStep = step;
        }

        if (id == CamPropId::Expo) {
            double freq = 1e6 / value;
            QString s;
            if (freq < 1000)
                s = QStringLiteral("Corresponds to <b>%1 Hz</b>").arg(freq, 0, 'f', 1);
            else
                s = QStringLiteral("Corresponds to <b>%1 kHz</b>").arg(freq/1000.0, 0, 'f', 2);
            auxLabel->setText(s);
        }
    }
    
    void applyValue()
    {
        double newValue = edit->value();
        if (Double(curValue).almostEqual(Double(newValue)))
            return;

        auto res = setValue(newValue);
        if (res) {
            Ori::Dlg::Defer::error(DllAdapter::errorText(res));
            return;
        } 

        showValue();

        if (onChanged)
            onChanged();
    }
    
    void applyValueFast(bool wheel, bool inc, bool big)
    {
        double changeFactor = wheel
            ? (big ? settings->changeWheelBig : settings->changeWheelSm)
            : (big ? settings->changeArrowBig : settings->changeArrowSm);

        double newValue;
        if (inc) {
            if (curValue >= maxValue) return;
            newValue = curValue * changeFactor;
            if (newValue - curValue < valueStep)
                newValue = curValue + valueStep;
            newValue = qMin(maxValue, newValue);
        } else {
            if (curValue <= minValue) return;
            newValue = curValue / changeFactor;
            if (curValue - newValue < valueStep)
                newValue = curValue - valueStep;
            newValue = qMax(minValue, newValue);
        }

        auto res = setValue(newValue);
        if (res) {
            Ori::Dlg::Defer::error(DllAdapter::errorText(res));
            return;
        } 

        showValue();

        if (onChanged)
            onChanged();
    }
};

} // namespace

//------------------------------------------------------------------------------
//                              DllControlPanelImpl
//------------------------------------------------------------------------------

class DllControlPanelImpl: public QWidget, public IAppSettingsListener
{
    Q_DECLARE_TR_FUNCTIONS(DllControlPanelImpl)

public:
    using Self = DllControlPanelImpl;

    DllControlPanelImpl(IBeamCamPtr camPtr, DllAdapter *adapter) 
        : QWidget(), camPtr(camPtr), adapter(adapter)
    {
        addPropControl_Expo(tr("Exposure (us)"));
        addPropControl_Fps(tr("Frame rate"));
        addPropControl_AGain(tr("Analog gain"));
        addPropControl_DGain(tr("Digital gain"));
        
        if (props.contains(CamPropId::Expo) && props.contains(CamPropId::Fps)) {
            // Exposition and FPS are interconnected properties
            // so when one of them is changed, the other can change
            props[CamPropId::Expo].onChanged = [this]{
                props[CamPropId::Fps].showValue();
                exposureChanged();
            };
            props[CamPropId::Fps].onChanged = [this]{
                props[CamPropId::Expo].showValue();
            };
        }
        else if (props.contains(CamPropId::Expo)) {
            props[CamPropId::Expo].onChanged = exposureChanged;
        }
        
        if (!props.isEmpty()) {
            auto layout = new QVBoxLayout(this);
            
            for (auto it = props.begin(); it != props.end(); it++) {
                it.value().makeWidgets(layout);
                groupBoxes << it.value().groupBox;
                
                if (it.value().id == CamPropId::Expo)
                    addAutoExpoControl(layout);
            }
                
            layout->addStretch();
        }
    }
    
#define ADD_PROP_CONTROL(Prop) \
    void addPropControl_##Prop(const QString& title) \
    { \
        auto res = adapter->get##Prop##Range(camPtr, nullptr, nullptr, nullptr); \
        if (res && res->code == IBeamStatus::IBEAM_STATUS_NOT_IMPLEMENTED) \
            return; \
        CamPropControl control; \
        control.id = CamPropId::Prop; \
        control.title = title; \
        control.settings = &propSettings; \
        control.getRange = [this](double* min, double* max, double* step) { \
            return adapter->get##Prop##Range(camPtr, min, max, step); }; \
        control.getValue = [this](double* value) { \
            return adapter->get##Prop(camPtr, value); }; \
        control.setValue = [this](double value) { \
            return adapter->set##Prop(camPtr, value); }; \
        props.insert(control.id, control); \
    }

    ADD_PROP_CONTROL(Expo);
    ADD_PROP_CONTROL(Fps);
    ADD_PROP_CONTROL(AGain);
    ADD_PROP_CONTROL(DGain);
    
    void addAutoExpoControl(QVBoxLayout *layout)
    {
        edAutoExpo = new CamPropEdit;
        edAutoExpo->connect(edAutoExpo, &ValueEdit::keyPressed, this, [this](int key){
            if (key == Qt::Key_Return || key == Qt::Key_Enter) autoExposure();
        });

        auto butAutoExpo = new QPushButton(tr("Find"));
        butAutoExpo->setFixedWidth(50);
        connect(butAutoExpo, &QPushButton::pressed, this, &Self::autoExposure);

        auto groupBox = LayoutV({
            tr("Percent of dynamic range:"),
            LayoutH({ edAutoExpo, butAutoExpo })
        }).makeGroupBox(tr("Autoexposure"));
        layout->addWidget(groupBox);
        groupBoxes << groupBox;

        auto actnCopyLog = new QAction(tr("Copy Report"), this);
        connect(actnCopyLog, &QAction::triggered, this, [this]{
            if (autoExpo) autoExpo->copyLog();
        });
        butAutoExpo->addAction(actnCopyLog);
        butAutoExpo->setContextMenuPolicy(Qt::ActionsContextMenu);
    }

#undef ADD_PROP_CONTROL
    
    void showInitialValues()
    {
        for (auto it = props.begin(); it != props.end(); it++)
            it.value().showValue();

        edAutoExpo->setValue(
            loadCamProp(DllControlPanel::StoredProp::AutoExpoTargetLevel).toInt());
    }
    
    void autoExposure()
    {
        if (!props.contains(CamPropId::Expo))
            return;
        auto &prop = props[CamPropId::Expo];
        if (prop.curValue == 0)
            return;
            
        int targetLevel = edAutoExpo->value();
        int clampedLevel = targetLevel;
        if (clampedLevel <= 0) clampedLevel = 1;
        else if (clampedLevel > 100) clampedLevel = 100;
        if (targetLevel != clampedLevel) {
            targetLevel = clampedLevel;
            edAutoExpo->setValue(targetLevel);
        }
        if (targetLevel != loadCamProp(DllControlPanel::StoredProp::AutoExpoTargetLevel).toInt())
            saveCamProp(DllControlPanel::StoredProp::AutoExpoTargetLevel, targetLevel);
        
        auto avgFrames = loadCamProp(DllControlPanel::StoredProp::AutoExpoAvgFrames).toInt();
        
        autoExpo = AutoExposure();
        autoExpo->logId = LOG_ID;
        autoExpo->targetLevel = targetLevel / 100.0;
        autoExpo->avgFrames = qMax(1, avgFrames);
        autoExpo->getExpoRange = [this](double* min, double* max, double* step){
            auto res = adapter->getExpoRange(camPtr, min, max, step);
            return res ? adapter->errorText(res) : QString();
        };
        autoExpo->setExpo = [this](double value){
            auto res = adapter->setExpo(camPtr, value);
            return res ? adapter->errorText(res) : QString();
        };
        autoExpo->getExpo = [this](double* value){
            auto res = adapter->getExpo(camPtr, value);
            return res ? adapter->errorText(res) : QString();
        };
        if (props.contains(CamPropId::Fps)) {
            autoExpo->getFpsRange = [this](double* min, double* max, double* step){
                auto res = adapter->getFpsRange(camPtr, min, max, step);
                return res ? adapter->errorText(res) : QString();
            };
            autoExpo->setFps = [this](double value){
                auto res = adapter->setFps(camPtr, value);
                return res ? adapter->errorText(res) : QString();
            };
            autoExpo->getFps = [this](double* value){
                auto res = adapter->getFps(camPtr, value);
                return res ? adapter->errorText(res) : QString();
            };
        }
        autoExpo->expoChanged = [this](double expo, double fps) {
            props[CamPropId::Expo].curValue = expo;
            props[CamPropId::Expo].edit->setValue(expo);
            if (props.contains(CamPropId::Fps)) {
                props[CamPropId::Fps].curValue = fps;
                props[CamPropId::Fps].edit->setValue(fps);
            }
        };
        autoExpo->stepRequested = [this]{
            watingBrightness = true;
            requestBrightness(this);
        };
        autoExpo->finished = [this]{ stopAutoExposure(); };
        
        for (auto groupBox : qAsConst(groupBoxes))
            groupBox->setDisabled(true);
            
        if (!autoExpo->start())
            stopAutoExposure();
    }
    
    void stopAutoExposure()
    {
        watingBrightness = false;

        for (auto groupBox : qAsConst(groupBoxes))
            groupBox->setDisabled(false);

        if (props.contains(CamPropId::Fps)) {
            auto &prop = props[CamPropId::Fps];
            if (auto res = adapter->setFps(camPtr, prop.curValue); res) {
                qWarning() << LOG_ID << "Failed to restore FPS after autoexposure" << adapter->errorText(res);
            }
            prop.showValue();
        }

        props[CamPropId::Expo].showValue();
        
        exposureChanged();
    }
    
    void applySettings(bool updateValues)
    {
        auto &s = AppSettings::instance();
        propSettings.changeWheelSm = 1 + double(s.propChangeWheelSm) / 100.0;
        propSettings.changeWheelBig = 1 + double(s.propChangeWheelBig) / 100.0;
        propSettings.changeArrowSm = 1 + double(s.propChangeArrowSm) / 100.0;
        propSettings.changeArrowBig = 1 + double(s.propChangeArrowBig) / 100.0;

        if (propSettings.roundFps != s.roundHardConfigFps) {
            propSettings.roundFps = s.roundHardConfigFps;
            if (updateValues && props.contains(CamPropId::Fps))
                props[CamPropId::Fps].showValue();
        }

        if (propSettings.roundExpo != s.roundHardConfigExp) {
           propSettings.roundExpo = s.roundHardConfigExp;
            if (updateValues && props.contains(CamPropId::Expo))
                props[CamPropId::Fps].showValue();
        }
    }
    
    // IAppSettingsListener
    void settingsChanged(bool affectsCamera) override
    {
        Q_UNUSED(affectsCamera)
        applySettings(true);
    }

    IBeamCamPtr camPtr;
    DllAdapter* adapter;
    QMap<CamPropId, CamPropControl> props;
    QList<QGroupBox*> groupBoxes;
    CamPropSettings propSettings;
    CamPropEdit* edAutoExpo;
    bool watingBrightness = false;
    bool closeRequested = false;
    std::function<void(QObject*)> requestBrightness;
    std::function<QVariant(DllControlPanel::StoredProp)> loadCamProp;
    std::function<void(DllControlPanel::StoredProp, QVariant)> saveCamProp;
    std::function<void()> exposureChanged;
    std::optional<AutoExposure> autoExpo;
    
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
            else if (autoExpo)
                autoExpo->doStep(e->level);
            return true;
        }
        return QWidget::event(event);
    }
};

//------------------------------------------------------------------------------
//                              DllControlPanel
//------------------------------------------------------------------------------

DllControlPanel::DllControlPanel(IBeamCamPtr camPtr, DllAdapter *adapter, 
    std::function<QVariant(StoredProp)> loadProp,
    std::function<void(StoredProp, QVariant)> saveProp,
    std::function<void(QObject*)> requestBrightness,
    std::function<void()> exposureChanged,
    QWidget *parent) : ControlPanel(parent)
{
    auto impl = new DllControlPanelImpl(camPtr, adapter);
    if (!impl->props.isEmpty())
    {
        impl->loadCamProp = loadProp;
        impl->saveCamProp = saveProp;
        impl->exposureChanged = exposureChanged;
        impl->requestBrightness = requestBrightness;
        impl->applySettings(false);
        impl->showInitialValues();
    
        auto scroll = new QScrollArea;
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidget(impl);
    
        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(scroll);
        
        _impl = impl;
    }
    else
    {
        impl->deleteLater();
        initForNoControls();
    }
}

void DllControlPanel::setReadOnly(bool on)
{
    if (!_impl)
        return;
        
    for (auto groupBox : std::as_const(_impl->groupBoxes))
        groupBox->setDisabled(on);
}
