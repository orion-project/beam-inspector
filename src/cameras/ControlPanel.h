#ifndef CONTROL_PANEL_H
#define CONTROL_PANEL_H

#include <QWidget>

class ControlPanel : public QWidget
{
    Q_OBJECT

public:
    ControlPanel(QWidget *parent);

    virtual void setReadOnly(bool) {}
    
    void initForNoControls();
};

#endif // CONTROL_PANEL_H
