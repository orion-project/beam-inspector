#ifndef HARD_CONFIG_PANEL_H
#define HARD_CONFIG_PANEL_H

#include <QWidget>

class HardConfigPanel : public QWidget
{
    Q_OBJECT

public:
    HardConfigPanel(QWidget *parent);

    virtual void setReadOnly(bool) {}
};

class StubHardConfigPanel : public HardConfigPanel
{
    Q_OBJECT

public:
    StubHardConfigPanel(QWidget *parent);
};

#endif // HARD_CONFIG_PANEL_H
