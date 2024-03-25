#include "PlotWindow.h"

#include "app\HelpSystem.h"

#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>

PlotWindow::PlotWindow(QWidget *parent) : QMainWindow(parent)
{
    QMenu *menu;

    menu = menuBar()->addMenu(tr("File"));
    menu->addAction(tr("Exit"), this, &PlotWindow::close);

    menu = menuBar()->addMenu(tr("Help"));
    auto help = HelpSystem::instance();
    menu->addAction(QIcon(":/toolbar/home"), tr("Visit Homepage"), help, &HelpSystem::visitHomePage);
    menu->addAction(QIcon(":/toolbar/bug"), tr("Send Bug Report"), help, &HelpSystem::sendBugReport);

    statusBar()->addPermanentWidget(new QLabel(help->appVersion()));

    resize(800, 600);
}

PlotWindow::~PlotWindow()
{
}
