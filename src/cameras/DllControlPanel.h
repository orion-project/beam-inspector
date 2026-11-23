#ifndef DLL_CONTROL_PANEL_H
#define DLL_CONTROL_PANEL_H

#include "cameras/DllAdapter.h"
#include "cameras/ControlPanel.h"

class DllControlPanel : public ControlPanel
{
public:
    DllControlPanel(IBeamCamPtr camPtr, DllAdapter *adapter, QWidget *parent);

    void setReadOnly(bool on) override;
    
private:
    class DllControlPanelImpl *_impl = nullptr;
};

#endif // DLL_CONTROL_PANEL_H