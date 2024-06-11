#include "HardConfigPanel.h"

#include <QLabel>
#include <QVBoxLayout>

HardConfigPanel::HardConfigPanel(QWidget *parent) : QWidget(parent)
{

}

StubHardConfigPanel::StubHardConfigPanel(QWidget *parent) : HardConfigPanel(parent)
{
    auto icon = new QLabel;
    icon->setPixmap(QIcon(":/misc/dial").pixmap(140));
    icon->setAlignment(Qt::AlignHCenter);

    auto label = new QLabel(tr("There are no controls\nfor this type of camera"), this);
    label->setAlignment(Qt::AlignHCenter);
    label->setWordWrap(true);

    auto layout = new QVBoxLayout(this);
    layout->addStretch();
    layout->addWidget(icon);
    layout->addWidget(label);
    layout->addStretch();
}
