#ifndef DLL_HARD_CONFIG_H
#define DLL_HARD_CONFIG_H

#include "cameras/DllAdapter.h"
#include "cameras/HardConfigPanel.h"

class DllHardConfigPanel : public HardConfigPanel
{
public:
    DllHardConfigPanel(IBeamCamPtr camPtr, DllAdapter *adapter, QWidget *parent);

    void setReadOnly(bool on) override;
    
private:
    class DllHardConfigPanelImpl *_impl = nullptr;
};

#endif // DLL_HARD_CONFIG_H