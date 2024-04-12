#include "PlotWindow.h"

#include "app/HelpSystem.h"
#include "cameras/StillImageCamera.h"
#include "cameras/VirtualDemoCamera.h"
#include "cameras/WelcomeCamera.h"
#include "widgets/Plot.h"
#include "widgets/PlotIntf.h"
#include "widgets/TableIntf.h"

#include "helpers/OriWidgets.h"
#include "tools/OriMruList.h"
#include "tools/OriSettings.h"
#include "widgets/OriFlatToolBar.h"
#include "widgets/OriMruMenu.h"
#include "widgets/OriStatusBar.h"

#include <QAction>
#include <QActionGroup>
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
#include <QWindowStateChangeEvent>

enum StatusPanels
{
    STATUS_CAMERA,
    STATUS_SEPARATOR_1,
    STATUS_RESOLUTION,
    STATUS_APERTURE_ICON,
    STATUS_APERTURE,
    STATUS_SEPARATOR_2,
    STATUS_FPS,
    STATUS_SEPARATOR_3,
    STATUS_WARNING,

    STATUS_PANEL_COUNT,
};

PlotWindow::PlotWindow(QWidget *parent) : QMainWindow(parent)
{
    _mru = new Ori::MruFileList(this);
    connect(_mru, &Ori::MruFileList::clicked, this, &PlotWindow::openImage);

    createMenuBar();
    createToolBar();
    createStatusBar();
    createDockPanel();
    createPlot();
    updateActions(false);
    setCentralWidget(_plot);

    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, &PlotWindow::updateThemeColors);
    setThemeColors();

    _camera.reset((Camera*)new WelcomeCamera(_plotIntf, _tableIntf));
    showCamConfig(false);
    showFps(0);

    restoreState();
    resize(800, 600);

    _plot->setFocus();

    QTimer::singleShot(0, this, [this]{
        _camera->capture();
        _plot->prepare();
        dataReady();
    });
}

PlotWindow::~PlotWindow()
{
    storeState();
    delete _tableIntf;
}

void PlotWindow::restoreState()
{
    Ori::Settings s;

    _mru->setMaxCount(20);
    _mru->load(s.settings());

    s.beginGroup("Plot");
    bool useRainbow = s.value("rainbow", true).toBool();
    bool showBeamInfo = s.value("beamInfo", false).toBool();
    _actionRainbow->setChecked(useRainbow);
    _actionGrayscale->setChecked(not useRainbow);
    _actionBeamInfo->setChecked(showBeamInfo);
    _plot->setRainbowEnabled(useRainbow, false);
    _plot->setBeamInfoVisible(showBeamInfo, false);
}

void PlotWindow::storeState()
{
    Ori::Settings s;
    s.beginGroup("Plot");
    s.setValue("rainbow", _actionRainbow->isChecked());
    s.setValue("beamInfo", _actionBeamInfo->isChecked());
}

void PlotWindow::createMenuBar()
{
#define A_ Ori::Gui::action
#define M_ Ori::Gui::menu
    auto actnNew = A_(tr("New Window"), this, &PlotWindow::newWindow, ":/toolbar/new", QKeySequence::New);
    _actionOpen = A_(tr("Open Image..."), this, &PlotWindow::openImageDlg, ":/toolbar/open_img", QKeySequence::Open);
    auto actnExport = A_(tr("Export Image..."), this, [this]{ _plot->exportImageDlg(); }, ":/toolbar/export_img");
    auto actnClose = A_(tr("Exit"), this, &PlotWindow::close);
    auto menuFile = M_(tr("File"), {actnNew, 0, actnExport, _actionOpen, actnClose});
    new Ori::Widgets::MruMenuPart(_mru, menuFile, actnClose, this);
    menuBar()->addMenu(menuFile);

    _actionBeamInfo = A_(tr("Plot Beam Info"), this, [this]{ _plot->setBeamInfoVisible(_actionBeamInfo->isChecked(), true); });
    _actionBeamInfo->setCheckable(true);

    auto colorGroup = new QActionGroup(this);
    _actionGrayscale = A_(tr("Grayscale"), colorGroup, [this]{ _plot->setRainbowEnabled(false, true); });
    _actionRainbow = A_(tr("Rainbow"), colorGroup, [this]{ _plot->setRainbowEnabled(true, true); });
    _actionGrayscale->setCheckable(true);
    _actionRainbow->setCheckable(true);
    auto actnZoomFull = A_(tr("Zoom to Sensor"), this, [this]{ _plot->zoomFull(true); });
    _actionZoomAperture = A_(tr("Zoom to Aperture"), this, [this]{ _plot->zoomAperture(true); });
    menuBar()->addMenu(M_(tr("View"), {
        _actionBeamInfo, 0,
        _actionGrayscale, _actionRainbow, 0,
        actnZoomFull, _actionZoomAperture,
    }));

    _actionStart = A_(tr("Start Capture"), this, &PlotWindow::startCapture, ":/toolbar/start");
    _actionStop = A_(tr("Stop Capture"), this, &PlotWindow::stopCapture, ":/toolbar/stop");
    _actionEditAperture = A_(tr("Edit Soft Aperture"), this, [this]{ _plot->startEditAperture(); }, ":/toolbar/aperture");
    _actionUseAperture = A_(tr("Use Soft Aperture"), this, &PlotWindow::toggleAperture);
    _actionUseAperture->setCheckable(true);
    _actionCamConfig = A_(tr("Settings..."), this, &PlotWindow::editCamConfig, ":/toolbar/settings");
    //auto actnSelectBgColor = A_(tr("Select Background..."), this, [this]{ _plot->selectBackgroundColor(); });
    menuBar()->addMenu(M_(tr("Camera"), {_actionStart, _actionStop, 0,
        //actnSelectBgColor, 0,
        _actionEditAperture, _actionUseAperture, 0,
        _actionCamConfig
    }));

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
    tb->addSeparator();
    tb->addAction(_actionOpen);
    tb->addSeparator();
    tb->addAction(_actionCamConfig);
}

void PlotWindow::createStatusBar()
{
    _statusBar = new Ori::Widgets::StatusBar(STATUS_PANEL_COUNT);

    _statusBar->addVersionLabel();
    _statusBar->setText(STATUS_CAMERA, "VirtualDemo");
    _statusBar->setText(STATUS_RESOLUTION, "2592 × 2048 × 8bit");
    _statusBar->setText(STATUS_SEPARATOR_1, "|");
    _statusBar->setText(STATUS_SEPARATOR_2, "|");
    _statusBar->setText(STATUS_SEPARATOR_3, "|");
    _statusBar->setMargin(STATUS_SEPARATOR_1, 0, 0);
    _statusBar->setMargin(STATUS_SEPARATOR_2, 0, 0);
    _statusBar->setMargin(STATUS_SEPARATOR_3, 0, 0);
    _statusBar->setMargin(STATUS_APERTURE_ICON, 6, 0);
    _statusBar->setMargin(STATUS_APERTURE, 0, 6);
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
    auto makeHeader = [this, &row](const QString& title) {
        _table->setRowCount(row+1);
        auto it = new QTableWidgetItem(title);
        auto f = it->font();
        f.setBold(true);
        it->setFont(f);
        it->setTextAlignment(Qt::AlignCenter);
        _table->setItem(row, 0, it);
        _table->setSpan(row, 0, 1, 2);
        row++;
    };
    auto makeItem = [this, &row](const QString& title, QTableWidgetItem **headerItem = nullptr) {
        _table->setRowCount(row+1);
        auto it = new QTableWidgetItem(" " + title + " ");
        auto f = it->font();
        f.setBold(true);
        f.setPointSize(f.pointSize()+1);
        it->setFont(f);
        _table->setItem(row, 0, it);
        if (headerItem)
            *headerItem = it;

        it = new QTableWidgetItem("---");
        f.setBold(false);
        it->setFont(f);
        _table->setItem(row++, 1, it);
        return it;
    };
    _tableIntf = new TableIntf;
    makeHeader(tr(" Centroid "));
    _tableIntf->itXc = makeItem(tr("Center X"));
    _tableIntf->itYc = makeItem(tr("Center Y"));
    _tableIntf->itDx = makeItem(tr("Width X"));
    _tableIntf->itDy = makeItem(tr("Width Y"));
    _tableIntf->itPhi = makeItem(tr("Azimuth"));
    _tableIntf->itEps = makeItem(tr("Ellipticity"));
    makeHeader(tr(" Debug "));
    _tableIntf->itRenderTime = makeItem(tr("Render time"), &_itemRenderTime);
    _tableIntf->itCalcTime = makeItem(tr("Calc time"));

    auto dock = new QDockWidget(tr("Results"));
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dock->setFeatures(QDockWidget::DockWidgetMovable);
    dock->setWidget(_table);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
}

void PlotWindow::createPlot()
{
    _plot = new Plot;
    _plotIntf = _plot->plotIntf();
    connect(_plot, &Plot::apertureEdited, this, &PlotWindow::apertureEdited);
}

void PlotWindow::closeEvent(QCloseEvent* ce)
{
    QMainWindow::closeEvent(ce);
    auto thread = dynamic_cast<QThread*>(_camera.get());
    if (thread && thread->isRunning()) {
        stopCapture();
    }
}

void PlotWindow::changeEvent(QEvent* e)
{
    QMainWindow::changeEvent(e);
    if (e->type() == QEvent::WindowStateChange) {
        // resizeEvent is not called when window gets maximized or restored
        _plot->zoomAuto(true);
    }
}

void PlotWindow::newWindow()
{
    if (!QProcess::startDetached(qApp->applicationFilePath(), {}))
        qWarning() << "Unable to start another instance";
}

void PlotWindow::showFps(int fps)
{
    if (fps <= 0)
        _statusBar->setText(STATUS_FPS, QStringLiteral("FPS: NA"));
    else _statusBar->setText(STATUS_FPS, QStringLiteral("FPS: ") % QString::number(fps));
}

void PlotWindow::showCamConfig(bool replot)
{
    setWindowTitle(qApp->applicationName() +  " [" + _camera->name() + ']');
    _statusBar->setText(STATUS_CAMERA, _camera->name(), _camera->descr());
    _statusBar->setText(STATUS_RESOLUTION, _camera->resolutionStr());

    auto c = _camera->config();
    _plot->setAperture(c.aperture, replot);
    _actionUseAperture->setChecked(c.aperture.enabled);
    _actionZoomAperture->setVisible(c.aperture.enabled);
    _statusBar->setVisible(STATUS_APERTURE_ICON, c.aperture.enabled);
    _statusBar->setVisible(STATUS_APERTURE, c.aperture.enabled);
    if (c.aperture.enabled) {
        bool valid = c.aperture.isValid(_camera->width(), _camera->height());
        QString hint = valid ? tr("Soft aperture") : tr("Soft aperture is not valid");
        QString icon = valid ? ":/toolbar/aperture" : ":/toolbar/aperture_warn";
        _statusBar->setIcon(STATUS_APERTURE_ICON, icon);
        _statusBar->setHint(STATUS_APERTURE_ICON, hint);
        _statusBar->setText(STATUS_APERTURE, c.aperture.sizeStr());
        _statusBar->setHint(STATUS_APERTURE, hint);
    }

    if (!c.subtractBackground) {
        _statusBar->setIcon(STATUS_WARNING, ":/toolbar/exclame");
        _statusBar->setHint(STATUS_WARNING, tr("Background subtraction disabled"));
    } else _statusBar->clear(STATUS_WARNING);
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
        _plot->setThemeColors(Plot::SYSTEM, true);
    });
}

void PlotWindow::updateActions(bool started)
{
    _mru->setDisabled(started);
    _actionCamConfig->setDisabled(started);
    _actionEditAperture->setDisabled(started);
    _actionUseAperture->setDisabled(started);
    _actionOpen->setDisabled(started);
    _actionStart->setDisabled(started);
    _actionStart->setVisible(!started);
    _actionStop->setDisabled(!started);
    _actionStop->setVisible(started);
}

void PlotWindow::startCapture()
{
    _plot->stopEditAperture(false);
    _itemRenderTime->setText(tr(" Render time "));
    auto cam = new VirtualDemoCamera(_plotIntf, _tableIntf, this);
    connect(cam, &VirtualDemoCamera::ready, this, &PlotWindow::dataReady);
    connect(cam, &VirtualDemoCamera::stats, this, &PlotWindow::showFps);
    connect(cam, &VirtualDemoCamera::started, this, [this]{ _plot->prepare(); });
    connect(cam, &VirtualDemoCamera::finished, this, &PlotWindow::captureStopped);
    _camera.reset((Camera*)cam);
    _camera->capture();
    showCamConfig(false);
    updateActions(true);
}

void PlotWindow::stopCapture()
{
    auto thread = dynamic_cast<QThread*>(_camera.get());
    if (!thread) {
        qWarning() << "Current camera is not thread based";
        return;
    }
    qDebug() << "Stopping camera thread...";
    _actionStop->setDisabled(true);
    thread->requestInterruption();
    if (!thread->wait(5000)) {
        qDebug() << "Can not stop camera thread in timeout";
    }
}

void PlotWindow::captureStopped()
{
    updateActions(false);
    showFps(0);
}

void PlotWindow::dataReady()
{
    _tableIntf->showResult();
    _plotIntf->showResult();
    _plot->replot();
}

void PlotWindow::openImageDlg()
{
    auto cam = new StillImageCamera(_plotIntf, _tableIntf);
    if (cam->fileName().isEmpty()) {
        delete cam;
        return;
    }
    _camera.reset((Camera*)cam);
    processImage();
}

void PlotWindow::openImage(const QString& fileName)
{
    _camera.reset((Camera*)new StillImageCamera(_plotIntf, _tableIntf, fileName));
    processImage();
}

void PlotWindow::processImage()
{
    auto cam = dynamic_cast<StillImageCamera*>(_camera.get());
    if (!cam) {
        qWarning() << "Current camera is not StillImageCamera";
        return;
    }
    _plot->stopEditAperture(false);
    _plotIntf->cleanResult();
    _tableIntf->cleanResult();
    _itemRenderTime->setText(tr(" Load time "));
    _mru->append(cam->fileName());
    _camera->capture();
    showCamConfig(false); // _after_ run(), when image is already loaded
    _plot->prepare();
    dataReady();
    showFps(0);
}

void PlotWindow::editCamConfig()
{
    if (_camera->editConfig())
        configChanged();
}

void PlotWindow::configChanged()
{
    if (dynamic_cast<StillImageCamera*>(_camera.get()))
        processImage();
    else
        showCamConfig(true);
}

void PlotWindow::apertureEdited()
{
    _camera->setAperture(_plot->aperture());
    configChanged();
}

void PlotWindow::toggleAperture()
{
    bool on = _actionUseAperture->isChecked();
    if (_plot->isApertureEditing() && !on)
        _plot->stopEditAperture(false);
    _camera->toggleAperture(on);
    configChanged();
}
