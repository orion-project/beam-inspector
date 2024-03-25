#include "PlotWindow.h"

#include "app/HelpSystem.h"
#include "widgets/Plot.h"

#include <QApplication>
#include <QDockWidget>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>

PlotWindow::PlotWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(qApp->applicationName() + " [VirtualDemo]");

    createMenuBar();
    createStatusBar();
    createDockPanel();
    createPlot();

    setCentralWidget(_plot);
    resize(800, 600);
}

PlotWindow::~PlotWindow()
{
}

void PlotWindow::createMenuBar()
{
    QMenu *menu;

    menu = menuBar()->addMenu(tr("File"));
    menu->addAction(tr("Exit"), this, &PlotWindow::close);

    menu = menuBar()->addMenu(tr("Help"));
    auto help = HelpSystem::instance();
    menu->addAction(QIcon(":/toolbar/home"), tr("Visit Homepage"), help, &HelpSystem::visitHomePage);
    menu->addAction(QIcon(":/toolbar/bug"), tr("Send Bug Report"), help, &HelpSystem::sendBugReport);
}

void PlotWindow::createStatusBar()
{
    auto sb = statusBar();

    _labelCamera = new QLabel("Camera: VirtualDemo");
    _labelCamera->setContentsMargins(0, 0, 6, 0);
    sb->addWidget(_labelCamera);

    sb->addWidget(new QLabel("|"));

    _labelResolution = new QLabel("2592 × 2048 × 8bit");
    _labelResolution->setContentsMargins(6, 0, 6, 0);
    sb->addWidget(_labelResolution);

    sb->addWidget(new QLabel("|"));

    _labelFps = new QLabel("FPS: NA");
    _labelFps->setContentsMargins(6, 0, 6, 0);
    sb->addWidget(_labelFps);

    sb->addWidget(new QLabel("|"));

    sb->addPermanentWidget(new QLabel(qApp->applicationVersion()));
}

void PlotWindow::createDockPanel()
{
    auto panel = new QWidget;
    auto dock = new QDockWidget(tr("Control"));
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dock->setFloating(false);
    dock->setWidget(panel);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
}

void PlotWindow::createPlot()
{
    _plot = new Plot;
}
