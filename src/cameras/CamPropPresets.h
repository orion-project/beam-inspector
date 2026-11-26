#ifndef CAM_PROP_PRESETS_H
#define CAM_PROP_PRESETS_H

#include "cameras/CameraTypes.h"

QT_BEGIN_NAMESPACE
class QPushButton;
class QGroupBox;
QT_END_NAMESPACE

class CamPropPresets : public QObject
{
public:
    CamPropPresets(const char* logId, QObject* parent);

    void makeButtons();
    QGroupBox* groupBox() const { return _groupBox; }
    
    std::function<bool(CamProp::Id, double)> applyProp;
    std::function<double(CamProp::Id)> getPropValue;
    std::function<AnyRecords*()> loadPresets;
    std::function<void()> savePresets;
    std::function<void()> adjustParentHeight;
    
private:
    const char* _logId;
    QList<QPushButton*> _buttons;
    QGroupBox* _groupBox;
    
    void applyPreset();
    void showPresetInfo();
    void renamePreset();
    void updatePreset();
    void removePreset();
    void saveNewPreset();
    
    AnyRecord makePreset();
    QString formatPreset(const AnyRecord &preset);
    void adjustHeight();
};

#endif // EXPOSURE_PRESETS_H
