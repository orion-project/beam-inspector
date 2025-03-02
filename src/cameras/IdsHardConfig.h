#ifndef IDS_HARD_CONFIG_H
#define IDS_HARD_CONFIG_H
#ifdef WITH_IDS

#include <ids_peak_comfort_c/ids_peak_comfort_c.h>

#include "cameras/HardConfigPanel.h"

class IdsHardConfigPanel : public HardConfigPanel
{
public:
    enum CamProp { AUTOEXP_LEVEL, AUTOEXP_FRAMES_AVG, EXP_PRESETS };

    IdsHardConfigPanel(peak_camera_handle hCam,
        std::function<void(QObject*)> requestBrightness,
        std::function<QVariant(CamProp)> getCamProp,
        std::function<void(CamProp, QVariant)> setCamProp,
        std::function<void()> raisePowerWarning,
        QWidget *parent);

    void setReadOnly(bool on) override;

protected:
    bool event(QEvent *event) override;

private:
    class IdsHardConfigPanelImpl *_impl;
};

#endif // WITH_IDS
#endif // IDS_HARD_CONFIG_H
