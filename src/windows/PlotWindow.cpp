#include "PlotWindow.h"

#include "app/HelpSystem.h"
#include "cameras/TableIntf.h"
#include "cameras/VirtualDemoCamera.h"
#include "widgets/Plot.h"

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QDockWidget>
#include <QHeaderView>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>
#include <QTableWidget>
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

class DataTableWidget : public QTableWidget {
    QSize sizeHint() const override { return {200, 100}; }
};

void PlotWindow::createDockPanel()
{
    _table = new DataTableWidget;
    qDebug() << _table->sizeHint();
    _table->setColumnCount(2);
    _table->setHorizontalHeaderLabels({ tr("Name"), tr("Value") });
    _table->verticalHeader()->setVisible(false);
    _table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _table->setSelectionBehavior(QAbstractItemView::SelectRows);
    auto h = _table->horizontalHeader();
    h->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    h->setSectionResizeMode(1, QHeaderView::Stretch);
    h->setHighlightSections(false);

    int row = 0;
    auto makeItem = [this, &row](const QString& title, bool header=false) {
        _table->setRowCount(row+1);
        auto it = new QTableWidgetItem(title);
        auto f = it->font();
        f.setBold(true);
        if (!header)
            f.setPointSize(f.pointSize()+1);
        it->setFont(f);
        it->setBackground(palette().brush(QPalette::Button));
        _table->setItem(row, 0, it);

        if (header) {
            it->setTextAlignment(Qt::AlignCenter);
            _table->setSpan(row, 0, 1, 2);
        } else {
            it = new QTableWidgetItem("---");
            f.setBold(false);
            it->setFont(f);
            _table->setItem(row, 1, it);
        }
        row++;
        return it;
    };
    _tableIntf.reset(new TableIntf);
    makeItem(tr(" Centroid "), true);
    _tableIntf->itXc = makeItem(tr(" Center X "));
    _tableIntf->itYc = makeItem(tr(" Center Y "));
    _tableIntf->itDx = makeItem(tr(" Width X "));
    _tableIntf->itDy = makeItem(tr(" Width Y "));
    _tableIntf->itPhi = makeItem(tr(" Azimuth "));
    makeItem(tr(" Debug "), true);
    _tableIntf->itRenderTime = makeItem(tr(" Render time "));
    _tableIntf->itCalcTime = makeItem(tr(" Calc time "));
//    _tableIntf->itRenderTime->setToolTip("ms/frame");
//    _tableIntf->itCalcTime->setToolTip("ms/frame");

    auto dock = new QDockWidget(tr("Results"));
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dock->setFeatures(QDockWidget::DockWidgetMovable);
    dock->setWidget(_table);
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
    _cameraThread = new VirtualDemoCamera(_plot->graphIntf(), _tableIntf, this);
    connect(_cameraThread, &VirtualDemoCamera::ready, this, &PlotWindow::dataReady);
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

void PlotWindow::dataReady()
{
    _plot->replot();
    _tableIntf->showData();
}

void PlotWindow::statsReceived(int fps)
{
    _labelFps->setText(QStringLiteral("FPS: ") % QString::number(fps));
}
