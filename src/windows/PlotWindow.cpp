#include "PlotWindow.h"

#include "app/HelpSystem.h"
#include "cameras/StillImageCamera.h"
#include "cameras/TableIntf.h"
#include "cameras/VirtualDemoCamera.h"
#include "widgets/Plot.h"

#include "helpers/OriWidgets.h"
#include "tools/OriMruList.h"
#include "widgets/OriFlatToolBar.h"
#include "widgets/OriMruMenu.h"
#include "widgets/OriStatusBar.h"

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QDockWidget>
#include <QHeaderView>
#include <QLabel>
#include <QMenuBar>
#include <QProcess>
#include <QStatusBar>
#include <QStyleHints>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>

enum StatusPanels
{
    STATUS_CAMERA,
    STATUS_SEPARATOR_1,
    STATUS_RESOLUTION,
    STATUS_SEPARATOR_2,
    STATUS_FPS,

    STATUS_PANEL_COUNT,
};

PlotWindow::PlotWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(qApp->applicationName() + " [VirtualDemo]");

    _mru = new Ori::MruFileList(this);
    connect(_mru, &Ori::MruFileList::clicked, this, &PlotWindow::openImage);
    _mru->load();

    createMenuBar();
    createToolBar();
    createStatusBar();
    createDockPanel();
    createPlot();
    updateActions(false);
    setCentralWidget(_plot);

    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, &PlotWindow::updateThemeColors);
    setThemeColors();

    _plot->setFocus();

    resize(800, 600);
}

PlotWindow::~PlotWindow()
{
}

void PlotWindow::createMenuBar()
{
#define A_ Ori::Gui::action
#define M_ Ori::Gui::menu
    auto actnNew = A_(tr("New Window"), this, &PlotWindow::newWindow, ":/toolbar/new", QKeySequence::New);
    _actionOpen = A_(tr("Open Beam Image..."), this, &PlotWindow::openImageDlg, ":/toolbar/open", QKeySequence::Open);
    auto actnClose = A_(tr("Exit"), this, &PlotWindow::close);
    auto menuFile = M_(tr("File"), {actnNew, 0, _actionOpen, 0, actnClose});
    new Ori::Widgets::MruMenuPart(_mru, menuFile, actnClose, this);
    menuBar()->addMenu(menuFile);

    _actionStart = A_(tr("Start Capture"), this, &PlotWindow::startCapture, ":/toolbar/start");
    _actionStop = A_(tr("Stop Capture"), this, &PlotWindow::stopCapture, ":/toolbar/stop");
    menuBar()->addMenu(M_(tr("Camera"), {_actionStart, _actionStop}));

    auto m = menuBar()->addMenu(tr("Help"));
    auto help = HelpSystem::instance();
    m->addAction(QIcon(":/toolbar/home"), tr("Visit Homepage"), help, &HelpSystem::visitHomePage);
    m->addAction(QIcon(":/toolbar/bug"), tr("Send Bug Report"), help, &HelpSystem::sendBugReport);
#undef M_
#undef A_
}

void PlotWindow::createToolBar()
{
    auto tb = new Ori::Widgets::FlatToolBar;
    addToolBar(tb);
    tb->setMovable(false);
    tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    tb->addAction(_actionStart);
    tb->addAction(_actionStop);
}

void PlotWindow::createStatusBar()
{
    _statusBar = new Ori::Widgets::StatusBar(STATUS_PANEL_COUNT);
    _statusBar->addVersionLabel();
    _statusBar->setText(STATUS_CAMERA, "VirtualDemo");
    _statusBar->setText(STATUS_RESOLUTION, "2592 × 2048 × 8bit");
    _statusBar->setText(STATUS_SEPARATOR_1, "|");
    _statusBar->setText(STATUS_SEPARATOR_2, "|");
    showFps(0);
    setStatusBar(_statusBar);
}

class DataTableWidget : public QTableWidget {
    QSize sizeHint() const override { return {200, 100}; }
};

void PlotWindow::createDockPanel()
{
    _table = new DataTableWidget;
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
    _actionOpen->setDisabled(started);
    _actionStart->setDisabled(started);
    _actionStart->setVisible(!started);
    _actionStop->setDisabled(!started);
    _actionStop->setVisible(started);
}

void PlotWindow::startCapture()
{
    _cameraThread = new VirtualDemoCamera(_plot->graphIntf(), _tableIntf, this);
    connect(_cameraThread, &VirtualDemoCamera::ready, this, &PlotWindow::dataReady);
    connect(_cameraThread, &VirtualDemoCamera::stats, this, &PlotWindow::showFps);
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
    showFps(0);
    _cameraThread->deleteLater();
    _cameraThread = nullptr;
}

void PlotWindow::dataReady()
{
    _plot->replot();
    _tableIntf->showData();
}

void PlotWindow::showFps(int fps)
{
    if (fps <= 0)
        _statusBar->setText(STATUS_FPS, QStringLiteral("FPS: NA"));
    else _statusBar->setText(STATUS_FPS, QStringLiteral("FPS: ") % QString::number(fps));
}

void PlotWindow::setThemeColors()
{
    auto bg = palette().brush(QPalette::Button);
    for (int row = 0; row < _table->rowCount(); row++)
        _table->item(row, 0)->setBackground(bg);
}

void PlotWindow::updateThemeColors()
{
    // Right now new palette is not ready yet, it returns old colors
    QTimer::singleShot(100, this, [this]{
        setThemeColors();
        _plot->setThemeColors(true);
    });
}

void PlotWindow::newWindow()
{
    if (!QProcess::startDetached(qApp->applicationFilePath(), {}))
        qWarning() << "Unable to start another instance";
}

void PlotWindow::openImageDlg()
{
    auto info = StillImageCamera::start(_plot->graphIntf(), _tableIntf);
    if (!info) return;
    _mru->append(info->filePath);
    _statusBar->setText(STATUS_CAMERA, info->fileName, info->filePath);
    _statusBar->setText(STATUS_RESOLUTION, QString("%1 × %2 × %3bit").arg(info->width).arg(info->height).arg(info->bits));
    showFps(0);
    _plot->prepare();
    dataReady();
}

void PlotWindow::openImage(const QString& fileName)
{
    auto info = StillImageCamera::start(fileName, _plot->graphIntf(), _tableIntf);
    if (!info) return;
    _mru->append(info->filePath);
    _statusBar->setText(STATUS_CAMERA, info->fileName, info->filePath);
    _statusBar->setText(STATUS_RESOLUTION, QString("%1 × %2 × %3bit").arg(info->width).arg(info->height).arg(info->bits));
    showFps(0);
    _plot->prepare();
    dataReady();
}
