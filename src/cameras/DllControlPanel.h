#ifndef DLL_CONTROL_PANEL_H
#define DLL_CONTROL_PANEL_H

#include "cameras/DllAdapter.h"
#include "cameras/ControlPanel.h"

class DllControlPanel : public ControlPanel
{
public:
    enum class StoredProp { AutoExpoTargetLevel, AutoExpoAvgFrames };

    DllControlPanel(IBeamCamPtr camPtr, DllAdapter *adapter,
        std::function<QVariant(StoredProp)> loadProp,
        std::function<void(StoredProp, QVariant)> saveProp,
        std::function<void(QObject*)> requestBrightness,
        std::function<void()> exposureChanged,
        QWidget *parent);

    void setReadOnly(bool on) override;
    
private:
    class DllControlPanelImpl *_impl = nullptr;
};

#endif // DLL_CONTROL_PANEL_H