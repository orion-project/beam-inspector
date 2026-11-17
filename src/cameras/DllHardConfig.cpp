#include "DllHardConfig.h"

#include "app/AppSettings.h"
#include "widgets/PlotHelpers.h"

#include "core/OriFloatingPoint.h"
#include "helpers/OriDialogs.h"
#include "helpers/OriLayouts.h"
#include "widgets/OriValueEdit.h"

#include <QApplication>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QWheelEvent>

#define LOG_ID "DllHardConfig:"

using namespace Ori::Layouts;
using namespace Ori::Widgets;

namespace {

enum class CamPropId { Expo, Fps };

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
    QLabel *label, *auxLabel;
    CamPropEdit *edit;
    QGroupBox *group;
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
        
        auto btn = new QPushButton(qApp->tr("Set", "DllHardConfigPanel"));
        btn->setFixedWidth(50); \
        btn->connect(btn, &QPushButton::pressed, btn, [this]{ applyValue(); });
        
        group = LayoutV({label, LayoutH({edit, btn})}).makeGroupBox(title);
        
        layout->addWidget(group);
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
                label->setText(QString("<b>Min = %1, Max = %2</b>").arg(min, 0, 'f', 2).arg(max, 0, 'f', 2));
            } else if (id == CamPropId::Expo && settings->roundExpo) {
                min = qRound(min / 10.0) * 10;
                max = qRound(max / 10.0) * 10;
                label->setText(QString("<b>Min = %1, Max = %2</b>").arg(min, 0, 'f', 0).arg(max, 0, 'f', 0));
            } else {
                label->setText(QString("<b>Min = %1, Max = %2</b>").arg(min, 0, 'f', 2).arg(max, 0, 'f', 2));
            }
            minValue = min;
            maxValue = max;
            valueStep = step;
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
//                             DllHardConfigPanelImpl
//------------------------------------------------------------------------------

class DllHardConfigPanelImpl: public QWidget, public IAppSettingsListener
{
public:
    DllHardConfigPanelImpl(IBeamCamPtr camPtr, DllAdapter *adapter) 
        : QWidget(), camPtr(camPtr), adapter(adapter)
    {
        addPropControl_Expo();
        
        if (!props.isEmpty())
        {
            auto layout = new QVBoxLayout(this);
            
            for (auto it = props.begin(); it != props.end(); it++)
                it.value().makeWidgets(layout);
                
            layout->addStretch();
        }
    }
    
    void addPropControl_Expo()
    {
        auto res = adapter->getExpoRange(camPtr, nullptr, nullptr, nullptr);
        if (res && res->code == IBeamStatus::IBEAM_STATUS_NOT_IMPLEMENTED)
            return;
        CamPropControl control;
        control.id = CamPropId::Expo;
        control.title = tr("Exposure (us)");
        control.settings = &propSettings;
        control.getRange = [this](double* min, double* max, double* step) {
            return adapter->getExpoRange(camPtr, min, max, step); };
        control.getValue = [this](double* value) {
            return adapter->getExpo(camPtr, value); };
        control.setValue = [this](double value) {
            return adapter->setExpo(camPtr, value); };
        control.onChanged = exposureChanged;
        props.insert(control.id, control);
    }
    
    void showInitialValues()
    {
        for (auto it = props.begin(); it != props.end(); it++)
            it.value().showValue();
    }
    
    // IAppSettingsListener
    void settingsChanged(bool affectsCamera) override
    {
        Q_UNUSED(affectsCamera)
        applySettings(true);
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

    IBeamCamPtr camPtr;
    DllAdapter* adapter;
    QHash<CamPropId, CamPropControl> props;
    CamPropSettings propSettings;
    std::function<void()> exposureChanged;    
};

//------------------------------------------------------------------------------
//                              DllHardConfigPanel
//------------------------------------------------------------------------------

DllHardConfigPanel::DllHardConfigPanel(IBeamCamPtr camPtr, DllAdapter *adapter, 
    QWidget *parent) : HardConfigPanel(parent)
{
    auto impl = new DllHardConfigPanelImpl(camPtr, adapter);
    if (!impl->props.isEmpty())
    {
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

void DllHardConfigPanel::setReadOnly(bool on)
{
    if (!_impl)
        return;
        
    for (auto it = _impl->props.begin(); it != _impl->props.end(); it++)
        it.value().group->setDisabled(on);
}
