#include "PlotWindow.h"

#include "app/HelpSystem.h"
#include "cameras/VirtualDemoCamera.h"
#include "widgets/Plot.h"

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QDockWidget>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>

PlotWindow::PlotWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(qApp->applicationName() + " [VirtualDemo]");
    createMenuBar();
    createToolBar();
    createStatusBar();
    createDockPanel();
    createPlot();
    updateActions(false);
    setCentralWidget(_plot);
    resize(800, 600);
}

PlotWindow::~PlotWindow()
{
}

void PlotWindow::createMenuBar()
{
    QMenu *m;

    m = menuBar()->addMenu(tr("Camera"));
    _actionStart = m->addAction(QIcon(":/toolbar/start"), tr("Start Capture"), this, &PlotWindow::startCapture);
    _actionStop = m->addAction(QIcon(":/toolbar/stop"), tr("Stop Capture"), this, &PlotWindow::stopCapture);
    m->addSeparator();
    m->addAction(tr("Exit"), this, &PlotWindow::close);

    m = menuBar()->addMenu(tr("Help"));
    auto help = HelpSystem::instance();
    m->addAction(QIcon(":/toolbar/home"), tr("Visit Homepage"), help, &HelpSystem::visitHomePage);
    m->addAction(QIcon(":/toolbar/bug"), tr("Send Bug Report"), help, &HelpSystem::sendBugReport);
}

class FlatToolBar : public QToolBar
{
protected:
    void paintEvent(QPaintEvent*) { /* do nothing */ }
};

void PlotWindow::createToolBar()
{
    auto tb = new FlatToolBar;
    addToolBar(tb);
    tb->setMovable(false);
    tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    tb->addAction(_actionStart);
    tb->addAction(_actionStop);
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

    _labelFps = new QLabel(QStringLiteral("FPS: NA"));
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

void PlotWindow::closeEvent(QCloseEvent* ce)
{
    QMainWindow::closeEvent(ce);
    if (_cameraThread && _cameraThread->isRunning()) {
        stopCapture();
    }
}

void PlotWindow::updateActions(bool started)
{
    _actionStart->setDisabled(started);
    _actionStart->setVisible(!started);
    _actionStop->setDisabled(!started);
    _actionStop->setVisible(started);
}

void PlotWindow::startCapture()
{
    _cameraThread = new VirtualDemoCamera(_plot->graphIntf(), this);
    connect(_cameraThread, &VirtualDemoCamera::ready, this, [this]{ _plot->replot(); });
    connect(_cameraThread, &VirtualDemoCamera::stats, this, &PlotWindow::statsReceived);
    connect(_cameraThread, &VirtualDemoCamera::started, this, [this]{ _plot->prepare(); });
    connect(_cameraThread, &VirtualDemoCamera::finished, this, &PlotWindow::captureStopped);
    _cameraThread->start();
    updateActions(true);
}

void PlotWindow::stopCapture()
{
    qDebug() << "Stopping camera thread...";
    _actionStop->setDisabled(true);
    _cameraThread->requestInterruption();
    if (!_cameraThread->wait(5000)) {
        qDebug() << "Can not stop camera thread in timeout";
    }
}

void PlotWindow::captureStopped()
{
    updateActions(false);
    _labelFps->setText(QStringLiteral("FPS: NA"));
    _cameraThread->deleteLater();
    _cameraThread = nullptr;
}

void PlotWindow::statsReceived(const double& fps)
{
    _labelFps->setText(QStringLiteral("FPS: ") % QString::number(fps, 'f', 1));
}
