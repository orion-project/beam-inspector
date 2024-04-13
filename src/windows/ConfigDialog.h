#ifndef CONFIG_DIALOG_H
#define CONFIG_DIALOG_H

#include "dialogs/OriBasicConfigDlg.h"

class AppSettingsDialog : public Ori::Dlg::BasicConfigDialog
{
    Q_OBJECT

public:
    AppSettingsDialog(QWidget* parent, Ori::Dlg::PageId currentPageId);

    // inherited from BasicConfigDialog
    virtual void populate() override;
    virtual bool collect() override;
};

#endif // CONFIG_DIALOG_H
