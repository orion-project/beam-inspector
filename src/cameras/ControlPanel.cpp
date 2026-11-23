#include "ControlPanel.h"

#include <QIcon>
#include <QLabel>
#include <QVBoxLayout>

ControlPanel::ControlPanel(QWidget *parent) : QWidget(parent)
{

}

void ControlPanel::initForNoControls()
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
