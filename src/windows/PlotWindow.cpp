#include "PlotWindow.h"

#include "app/AppSettings.h"
#include "app/HelpSystem.h"
#ifdef WITH_IDS
    #include "cameras/IdsCamera.h"
#endif
#include "cameras/HardConfigPanel.h"
#include "cameras/MeasureSaver.h"
#include "cameras/StillImageCamera.h"
#include "cameras/VirtualDemoCamera.h"
#include "cameras/WelcomeCamera.h"
#include "helpers/OriDialogs.h"
#include "plot/PlotExport.h"
#include "widgets/Plot.h"
#include "widgets/PlotIntf.h"
#include "widgets/TableIntf.h"

#include "helpers/OriDialogs.h"
#include "helpers/OriWidgets.h"
#include "tools/OriMruList.h"
#include "tools/OriSettings.h"
#include "widgets/OriFlatToolBar.h"
#include "widgets/OriMruMenu.h"
#include "widgets/OriPopupMessage.h"
#include "widgets/OriStatusBar.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QMenuBar>
#include <QProcess>
#include <QProgressBar>
#include <QSpinBox>
#include <QStatusBar>
#include <QStyleHints>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QToolTip>
#include <QUrl>
#include <QWindowStateChangeEvent>

#define LOG_ID "PlotWindow:"
#define INI_GROUP_CAM_NAMES "CameraNames"

enum StatusPanels
{
    STATUS_CAMERA,
    STATUS_SEPARATOR_1,
    STATUS_RESOLUTION,
    STATUS_ROI_ICON,
    STATUS_ROI,
    STATUS_SEPARATOR_2,
    STATUS_FPS,
    STATUS_SEPARATOR_3,
    STATUS_NO_DATA,
    STATUS_BGND,

    STATUS_PANEL_COUNT,
};

//------------------------------------------------------------------------------
//                             MeasureProgressBar
//------------------------------------------------------------------------------

class MeasureProgressBar : public QProgressBar
{
public:
    void reset(int duration, const QString &fileName)
    {
        setElapsed(0);
        setMaximum(duration);
        setVisible(true);
        setFormat("%p%");
        _fileName = fileName;
    }

    void setElapsed(qint64 ms) {
        _secs = ms / 1000;
        setValue(_secs);
        if (value() >= maximum()) {
            setFormat(tr("Finishing..."));
        }
    }

protected:
    bool event(QEvent *e) override {
        if (e->type() != QEvent::ToolTip)
            return QProgressBar::event(e);
        if (auto he = dynamic_cast<QHelpEvent*>(e); he)
            QToolTip::showText(he->globalPos(), formatTooltip());
        return true;
    }

    void contextMenuEvent(QContextMenuEvent *e) override {
        if (!_contextMenu) {
            _contextMenu = new QMenu(this);
            _contextMenu->addAction(tr("Open file location"), this, &MeasureProgressBar::openFileLocation);
        }
        _contextMenu->popup(e->globalPos());
    }

private:
    int _secs;
    QString _fileName;
    QMenu *_contextMenu = nullptr;

    QString formatTooltip() const {
        int max = maximum();
        if (max == 0)
            return tr("Measurements"
                "<br>Elapsed: <b>%1</b>"
                "<br>File: <b>%2</b>")
                .arg(formatSecs(_secs), _fileName);
        return tr("Measurements"
            "<br>Duration: <b>%1</b>"
            "<br>Elapsed: <b>%2</b>"
            "<br>Remaining: <b>%3</b>"
            "<br>File: <b>%4</b>")
            .arg(formatSecs(max), formatSecs(_secs), formatSecs(max - _secs), _fileName);
    }

    void openFileLocation() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(_fileName).dir().absolutePath()));
    }
};

//------------------------------------------------------------------------------
//                               PlotWindow
//------------------------------------------------------------------------------

PlotWindow::PlotWindow(QWidget *parent) : QMainWindow(parent)
{
    setObjectName("PlotWindow");

    Ori::Gui::PopupMessage::setTarget(this);

    _mru = new Ori::MruFileList(this);
    connect(_mru, &Ori::MruFileList::clicked, this, &PlotWindow::openImage);

    createPlot();
    createMenuBar();
    createToolBar();
    createStatusBar();
    createDockPanel();
    setCentralWidget(_plot);
    fillCamSelector();

    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, &PlotWindow::updateThemeColors);
    setThemeColors();

    restoreState();

    _plot->setFocus();

    QTimer::singleShot(0, this, [this]{
        // dock still is not visible when asking in resoreState
        _actionResultsPanel->setChecked(_resultsDock->isVisible());
        _actionHardConfig->setChecked(_hardConfigDock->isVisible());

        // This initializes all graph structs
        activateCamWelcome();

        if (!AppSettings::instance().isDevMode)
            openImage({});

        HelpSystem::instance()->checkForUpdatesAuto();
    });
}

PlotWindow::~PlotWindow()
{
    storeState();
    delete _tableIntf;

#ifdef WITH_IDS
    // Close camera explicitly, otherwise it gets closed after the lib unloaded
    _camera.reset();
    IdsCamera::unloadLib();
#endif
}

void PlotWindow::restoreState()
{
    Ori::Settings s;
    s.restoreWindowGeometry(this);
    s.restoreDockState(this);

    _mru->setMaxCount(20);
    _mru->load(s.settings());

    s.beginGroup("Plot");
    bool showBeamInfo = s.value("beamInfo", false).toBool();
    bool rawView = s.value("rawView", false).toBool();
    s.endGroups();

    _actionBeamInfo->setChecked(showBeamInfo);
    _actionRawView->setChecked(rawView);
    _plot->setRawView(rawView, false);
    _plot->setColorMap(AppSettings::instance().currentColorMap(), false);
    _plot->setBeamInfoVisible(showBeamInfo, false);
    _plot->restoreState(s.settings());
    _actionCrosshairsShow->setChecked(_plot->isCrosshairsVisible());
}

void PlotWindow::storeState()
{
    Ori::Settings s;
    s.storeWindowGeometry(this);
    s.storeDockState(this);

    s.beginGroup("Plot");
    s.setValue("beamInfo", _actionBeamInfo->isChecked());
    s.setValue("rawView", _actionRawView->isChecked());
    s.endGroups();

    _plot->storeState(s.settings());
}

void PlotWindow::createMenuBar()
{
#define A_ Ori::Gui::action
#define M_ Ori::Gui::menu
    if (AppSettings::instance().isDevMode)
    {
        _actionCamWelcome = A_("Welcome", this, &PlotWindow::activateCamWelcome, ":/toolbar/camera");
        _actionCamDemo = A_("Demo", this, &PlotWindow::activateCamDemo, ":/toolbar/camera");
    }
    _actionCamImage = A_("Image", this, &PlotWindow::activateCamImage, ":/toolbar/camera");
    _actionRefreshCams = A_("Refresh", this, &PlotWindow::fillCamSelector);
    _camSelectMenu = new QMenu(tr("Active Camera"), this);

    auto actnNew = A_(tr("New Window"), this, &PlotWindow::newWindow, ":/toolbar/new", QKeySequence::New);
    _actionOpenImg = A_(tr("Open Image..."), this, &PlotWindow::openImageDlg, ":/toolbar/open_img", QKeySequence::Open);
    _actionSaveRaw = A_(tr("Export Raw Image..."), this, [this]{ _camera->requestRawImg(this); }, ":/toolbar/save_raw", QKeySequence("F6"));
    auto actnSaveImg = A_(tr("Export Plot Image..."), this, [this]{ _plot->exportImageDlg(); }, ":/toolbar/save_img", QKeySequence("F7"));
    auto actnClose = A_(tr("Exit"), this, &PlotWindow::close);
    auto actnPrefs = A_(tr("Preferences..."), this, [this]{ AppSettings::instance().edit(); }, ":/toolbar/options");
    auto menuFile = M_(tr("File"), {
        actnNew,
        0, _actionSaveRaw, actnSaveImg,
        0, _actionOpenImg, new Ori::Widgets::MruMenu(_mru),
        0, actnPrefs,
        0, actnClose});
    menuBar()->addMenu(menuFile);

    _actionRawView = A_(tr("Raw View"), this, &PlotWindow::toggleRawView, ":/toolbar/raw_view", Qt::Key_F12);
    _actionRawView->setCheckable(true);
    _actionBeamInfo = A_(tr("Plot Beam Info"), this, [this]{ _plot->setBeamInfoVisible(_actionBeamInfo->isChecked(), true); });
    _actionBeamInfo->setCheckable(true);

    _actionZoomFull = A_(tr("Zoom to Sensor"), this, [this]{ _plot->zoomFull(true); }, ":/toolbar/zoom_sensor", QKeySequence("Ctrl+0"));
    _actionZoomRoi = A_(tr("Zoom to ROI"), this, [this]{ _plot->zoomRoi(true); }, ":/toolbar/zoom_roi", QKeySequence("Ctrl+1"));
    _colorMapMenu = new QMenu(tr("Color Map"));
    _actionLoadColorMap = A_(tr("Load From File..."), this, &PlotWindow::selectColorMapFile);
    _actionCleanColorMaps = A_(tr("Delete Invalid Items"), this, []{ AppSettings::instance().deleteInvalidColorMaps(); });
    connect(_colorMapMenu, &QMenu::aboutToShow, this, &PlotWindow::updateColorMapMenu);
    _colorMapActions = new QActionGroup(this);
    _colorMapActions->setExclusive(true);
    _actionResultsPanel = A_(tr("Results Panel"), this, &PlotWindow::toggleResultsPanel, ":/toolbar/table");
    _actionResultsPanel->setCheckable(true);
    _actionHardConfig = A_(tr("Device Control"), this, &PlotWindow::toggleHardConfig, ":/toolbar/hardware");
    _actionHardConfig->setCheckable(true);
    auto menuView = M_(tr("View"), {
        _actionRawView, 0,
        _actionResultsPanel, _actionHardConfig, 0,
        _actionBeamInfo, _colorMapMenu, 0,
        _actionZoomFull, _actionZoomRoi,
    });
    if (AppSettings::instance().isDevMode) {
        menuView->addSeparator();
        menuView->addAction(tr("Resize Main Window..."), this, &PlotWindow::devResizeWindow);
        menuView->addAction(tr("Resize Results Panel..."), this, [this]{ devResizeDock(_resultsDock); });
        menuView->addAction(tr("Resize Control Panel..."), this, [this]{ devResizeDock(_hardConfigDock); });
    }

    menuBar()->addMenu(menuView);

    _actionMeasure = A_(tr("Start Measurements"), this, &PlotWindow::toggleMeasure, ":/toolbar/start", Qt::Key_F9);
    _actionEditRoi = A_(tr("Edit ROI"), this, [this]{ _plot->startEditRoi(); }, ":/toolbar/roi");
    _actionUseRoi = A_(tr("Use ROI"), this, &PlotWindow::toggleRoi, ":/toolbar/roi_rect");
    _actionUseRoi->setCheckable(true);
    _actionSetupPowerMeter = A_(tr("Power Meter..."), this, [this]{ _camera->setupPowerMeter(); });
    _actionSetCamCustomName = A_(tr("Custom Name..."), this, &PlotWindow::setCamCustomName);
    _actionCamConfig = A_(tr("Settings..."), this, [this]{ PlotWindow::editCamConfig(-1); }, ":/toolbar/settings");
    menuBar()->addMenu(M_(tr("Camera"), {
        _actionMeasure, 0,
        _actionEditRoi, _actionUseRoi, 0,
        _actionSetupPowerMeter, _actionSetCamCustomName, _actionCamConfig,
    }));

    _actionCrosshairsShow = A_(tr("Show Crosshairs"), this, &PlotWindow::toggleCrosshairsVisbility, ":/toolbar/crosshair");
    _actionCrosshairsShow->setCheckable(true);
    _actionCrosshairsEdit = A_(tr("Edit Crosshairs"), this, &PlotWindow::toggleCrosshairsEditing, ":/toolbar/crosshair_edit");
    _actionCrosshairsEdit->setCheckable(true);
    auto actnClearCrosshairs = A_(tr("Clear Crosshairs"), _plot, &Plot::clearCrosshairs, ":/toolbar/trash");
    auto actnLoadCrosshairs = A_(tr("Load From File..."), _plot, qOverload<>(&Plot::loadCrosshairs), ":/toolbar/open");
    auto actnSaveCrosshairs = A_(tr("Save To File..."), _plot, &Plot::saveCrosshairs, ":/toolbar/save");
    auto menuOverlays = M_(tr("Overlays"), {
        _actionCrosshairsShow, 0, _actionCrosshairsEdit, actnClearCrosshairs,
        0, actnLoadCrosshairs, actnSaveCrosshairs,
    });
    new Ori::Widgets::MruMenuPart(_plot->mruCrosshairs(), menuOverlays, actnLoadCrosshairs, this);
    menuBar()->addMenu(menuOverlays);

    auto m = menuBar()->addMenu(tr("Help"));
    auto help = HelpSystem::instance();
    m->addAction(QIcon(":/ori_images/help"), tr("Help"), QKeySequence::HelpContents, help, &HelpSystem::showContent);
    m->addAction(QIcon(":/toolbar/home"), tr("Visit Homepage"), help, &HelpSystem::visitHomePage);
    m->addAction(QIcon(":/toolbar/bug"), tr("Send Bug Report"), help, &HelpSystem::sendBugReport);
    m->addAction(tr("Check For Updates"), help, &HelpSystem::checkForUpdates);
    m->addSeparator();
    m->addAction(tr("About..."), help, &HelpSystem::showAbout);
#undef M_
#undef A_
}

void PlotWindow::fillCamSelector()
{
    _camSelectMenu->clear();
    if (AppSettings::instance().isDevMode)
        _camSelectMenu->addAction(_actionCamWelcome);
    _camSelectMenu->addAction(_actionCamImage);
    if (AppSettings::instance().isDevMode)
        _camSelectMenu->addAction(_actionCamDemo);

#ifdef WITH_IDS
    Ori::Settings s;
    s.beginGroup(INI_GROUP_CAM_NAMES);
    _camCustomNames.clear();
    if (auto err = IdsCamera::libError(); !err.isEmpty()) {
        _camSelectMenu->addAction(QIcon(":/toolbar/error"), tr("IDS Camera"), this, [err]{
            Ori::Dlg::error(err);
        });
    } else {
        for (const auto& cam : IdsCamera::getCameras()) {
            auto name = cam.displayName;
            auto customName = s.value(cam.customId).toString();
            if (!customName.isEmpty()) {
                _camCustomNames.insert(cam.customId, customName);
                name = customName;
            }
            auto a = _camSelectMenu->addAction(QIcon(":/toolbar/camera"), name, this, &PlotWindow::activateCamIds);
            a->setData(cam.cameraId);
        }
    }
#endif

    _camSelectMenu->addSeparator();
    _camSelectMenu->addAction(_actionRefreshCams);
}

void PlotWindow::createToolBar()
{
    _buttonSelectCam = new QToolButton;
    _buttonSelectCam->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    _buttonSelectCam->setPopupMode(QToolButton::InstantPopup);
    _buttonSelectCam->setIcon(QIcon(":/toolbar/camera"));
    _buttonSelectCam->setMenu(_camSelectMenu);
    _buttonSelectCam->setStyleSheet("QToolButton{font-weight: bold}");
    _buttonSelectCam->setToolTip(tr("Active camera"));

    auto tb = new Ori::Widgets::FlatToolBar;
    tb->setObjectName("ToolBarMain");
    addToolBar(tb);
    tb->setMovable(false);
    tb->addWidget(_buttonSelectCam);
    tb->addAction(_actionCamConfig);
    tb->addSeparator();
    tb->addAction(_actionRawView);
    tb->addSeparator();
    tb->addAction(_actionResultsPanel);
    tb->addAction(_actionHardConfig);
    tb->addSeparator();
    tb->addAction(_actionEditRoi);
    tb->addAction(_actionUseRoi);
    tb->addSeparator();
    _buttonMeasure = tb->addWidget(Ori::Gui::textToolButton(_actionMeasure));
    _buttonOpenImg = tb->addWidget(Ori::Gui::textToolButton(_actionOpenImg));
    tb->addSeparator();
    tb->addAction(_actionZoomFull);
    tb->addAction(_actionZoomRoi);
    tb->addSeparator();
    tb->addAction(_actionCrosshairsShow);
    tb->addAction(_actionCrosshairsEdit);
}

void PlotWindow::createStatusBar()
{
    _statusBar = new Ori::Widgets::StatusBar(STATUS_PANEL_COUNT);
    _statusBar->addVersionLabel();
    _statusBar->setText(STATUS_SEPARATOR_1, "|");
    _statusBar->setText(STATUS_SEPARATOR_2, "|");
    _statusBar->setText(STATUS_SEPARATOR_3, "|");
    _statusBar->setMargin(STATUS_SEPARATOR_1, 0, 0);
    _statusBar->setMargin(STATUS_SEPARATOR_2, 0, 0);
    _statusBar->setMargin(STATUS_SEPARATOR_3, 0, 0);
    _statusBar->setMargin(STATUS_ROI_ICON, 6, 0);
    _statusBar->setMargin(STATUS_ROI, 0, 6);
    _statusBar->setDblClick(STATUS_ROI, [this]{ editCamConfig(Camera::cfgRoi); });
    _statusBar->setDblClick(STATUS_ROI_ICON, [this]{ editCamConfig(Camera::cfgRoi); });
    _statusBar->setIcon(STATUS_NO_DATA, ":/toolbar/error");
    _statusBar->setHint(STATUS_NO_DATA, tr("No data to process.\n"
        "All pixels in the region are below noise threshold"));
    _statusBar->setIcon(STATUS_BGND, ":/toolbar/exclame");
    _statusBar->setHint(STATUS_BGND, tr("Background subtraction disabled.\n"
        "The measurement is not compliant with the ISO standard."));
    _statusBar->setDblClick(STATUS_BGND, [this]{ editCamConfig(Camera::cfgBgnd); });

    _measureProgress = new MeasureProgressBar;
    _measureProgress->setVisible(false);
    _statusBar->addWidget(_measureProgress);

    setStatusBar(_statusBar);
}

class DataTableWidget : public QTableWidget {
    QSize sizeHint() const override { return {200, 100}; }
};

void PlotWindow::createDockPanel()
{
    _table = new DataTableWidget;
    _tableIntf = new TableIntf(_table);
    connect(_table, &QTableWidget::itemDoubleClicked, this, &PlotWindow::resultsTableDoubleClicked);

    _resultsDock = new QDockWidget(tr("Results"));
    _resultsDock->setObjectName("DockResults");
    _resultsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    _resultsDock->setFeatures(QDockWidget::DockWidgetMovable);
    _resultsDock->setWidget(_table);
    addDockWidget(Qt::LeftDockWidgetArea, _resultsDock);

    _stubConfigPanel = new StubHardConfigPanel(this);
    _camConfigPanel = _stubConfigPanel;
    _hardConfigDock = new QDockWidget(tr("Control"));
    _hardConfigDock->setObjectName("DockHardConfig");
    _hardConfigDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    _hardConfigDock->setFeatures(QDockWidget::DockWidgetMovable);
    _hardConfigDock->setWidget(_stubConfigPanel);
    addDockWidget(Qt::RightDockWidgetArea, _hardConfigDock);
}

void PlotWindow::createPlot()
{
    _plot = new Plot;
    _plotIntf = _plot->plotIntf();
    connect(_plot, &Plot::roiEdited, this, &PlotWindow::roiEdited);
}

void PlotWindow::closeEvent(QCloseEvent* ce)
{
    QMainWindow::closeEvent(ce);

    if (_saver) {
        if (!Ori::Dlg::yes(tr("Measurements still in progress.\n\nInterrupt?"))) {
            ce->ignore();
            return;
        }
        // could be finished while dialog opened
        if (_saver) toggleMeasure(true);
    }

    auto thread = dynamic_cast<QThread*>(_camera.get());
    if (thread && thread->isRunning()) {
        stopCapture();
    }
}

void PlotWindow::changeEvent(QEvent* e)
{
    QMainWindow::changeEvent(e);

    // resizeEvent is not called when window gets maximized or restored
    if (e->type() == QEvent::WindowStateChange)
        _plot->adjustWidgetSize();
}

bool PlotWindow::event(QEvent *event)
{
    if (auto e = dynamic_cast<ImageEvent*>(event); e) {
        exportImageDlg(e->buf, _camera->width(), _camera->height(), _camera->bpp() > 8);
        return true;
    }
    return QMainWindow::event(event);
}

void PlotWindow::newWindow()
{
    if (!QProcess::startDetached(qApp->applicationFilePath(), {}))
        qWarning() << "Unable to start another instance";
}

void PlotWindow::showFps(double fps, double hardFps)
{
    if (fps <= 0) {
        _statusBar->setText(STATUS_FPS, QStringLiteral("FPS: NA"));
        _statusBar->setHint(STATUS_FPS, {});
        return;
    }
    _statusBar->setText(STATUS_FPS, QStringLiteral("FPS: ") % QString::number(fps, 'f', 2));
    if (hardFps > 0 && qCeil(fps) < qFloor(hardFps)) {
        _statusBar->setHint(STATUS_FPS, tr("The system likely run out of CPU resources.\nActual FPS is lower than camera produces (%1).").arg(hardFps));
        _statusBar->setStyleSheet(STATUS_FPS, QStringLiteral("QLabel{background:red;font-weight:bold;color:white}"));
    } else {
        _statusBar->setHint(STATUS_FPS, {});
        _statusBar->setStyleSheet(STATUS_FPS, {});
    }
}

void PlotWindow::showSelectedCamera()
{
    bool isImage = dynamic_cast<StillImageCamera*>(_camera.get());
    auto name = _camCustomNames.value(_camera->customId(), _camera->name());
    _buttonSelectCam->setText("  " + (isImage ? _actionCamImage->text() : name));
    setWindowTitle(name + " - " + qApp->applicationName());
}

void PlotWindow::showCamConfig(bool replot)
{
    bool isImage = dynamic_cast<StillImageCamera*>(_camera.get());
    _buttonOpenImg->setVisible(isImage);
    _actionMeasure->setVisible(_camera->canMeasure());
    _buttonMeasure->setVisible(_camera->canMeasure());
    _actionSaveRaw->setEnabled(_camera->canSaveRawImg() && _camera->isCapturing());
    _actionSetCamCustomName->setVisible(!_camera->customId().isEmpty());
    _actionSetupPowerMeter->setVisible(_camera->isPowerMeter());
    showSelectedCamera();

    _statusBar->setText(STATUS_CAMERA, _camera->name(), _camera->descr());
    _statusBar->setText(STATUS_RESOLUTION, _camera->resolutionStr());

    const auto &c = _camera->config();
    _actionUseRoi->setChecked(c.roi.on);
    _actionZoomRoi->setVisible(c.roi.on);
    _statusBar->setVisible(STATUS_ROI_ICON, c.roi.on);
    _statusBar->setVisible(STATUS_ROI, c.roi.on);
    if (c.roi.on) {
        bool valid = _camera->isRoiValid();
        QString hint = valid ? tr("Region of interest") : tr("Region is not valid");
        QString icon = valid ? ":/toolbar/roi" : ":/toolbar/roi_warn";
        _statusBar->setIcon(STATUS_ROI_ICON, icon);
        _statusBar->setHint(STATUS_ROI_ICON, hint);
        _statusBar->setText(STATUS_ROI, c.roi.sizeStr(_camera->width(), _camera->height()));
        _statusBar->setHint(STATUS_ROI, hint);
    }

    _statusBar->setVisible(STATUS_BGND, !c.bgnd.on);

    auto s = _camera->pixelScale();
    _plot->setImageSize(_camera->width(), _camera->height(), s);
    if (c.roi.isZero())
        _plot->setRoi({false, 0, 0, 1, 1});
    else _plot->setRoi(c.roi);
    _tableIntf->setScale(s);
    _plotIntf->setScale(s);
    if (replot) _plot->replot();
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

void PlotWindow::updateControls()
{
    bool opened = _camera && _camera->isCapturing();
    bool started = (bool)_saver;
    _mru->setDisabled(started);
    _buttonSelectCam->setDisabled(started);
    _actionCamConfig->setDisabled(started || !opened);
    _actionEditRoi->setDisabled(started || !opened);
    _actionUseRoi->setDisabled(started || !opened);
    _actionOpenImg->setDisabled(started);
    _actionRawView->setDisabled(started || !opened);
    _actionMeasure->setText(started ? tr("Stop Measurements") : tr("Start Measurements"));
    _actionMeasure->setIcon(QIcon(started ? ":/toolbar/stop" : ":/toolbar/start"));
    _actionMeasure->setDisabled(!opened);
    _camConfigPanel->setReadOnly(started || !opened);
    _actionSetupPowerMeter->setDisabled(started || !opened);
}

void PlotWindow::toggleMeasure(bool force)
{
    auto cam = _camera.get();
    if (!cam) return;

    if (!cam->isCapturing()) {
        Ori::Gui::PopupMessage::warning(tr("Camera is not opened"));
        return;
    }

    if (_saver)
    {
        if (!force) {
            if (!Ori::Dlg::yes(tr("Interrupt measurements?")))
                return;
            // could be finished while dialog opened
            if (!_saver)
                return;
        }

        cam->stopMeasure();
        // Process the last MeasureEvent
        qApp->processEvents();
        _saver.reset(nullptr);
        _measureProgress->setVisible(false);
        updateControls();
        return;
    }

    auto cfg = MeasureSaver::configure();
    if (!cfg)
        return;

    _plot->setRawView(false, false);
    cam->setRawView(false, true);
    _actionRawView->setChecked(false);

    auto saver = new MeasureSaver();
    auto res = saver->start(*cfg, cam);
    if (!res.isEmpty()) {
        Ori::Dlg::error(tr("Failed to start measuments:\n%1").arg(res));
        return;
    }
    connect(saver, &MeasureSaver::finished, this, [this]{
        toggleMeasure(true);
        Ori::Gui::PopupMessage::affirm(tr("<b>Measurements finished<b>"), 0);
    });
    connect(saver, &MeasureSaver::interrupted, this, [this](const QString &error){
        toggleMeasure(true);
        Ori::Gui::PopupMessage::error(tr("<b>Measurements interrupted</b><p>") + QString(error).replace("\n", "<br>"), 0);
    });
    _saver.reset(saver);
    Ori::Gui::PopupMessage::cancel();
    cam->startMeasure(_saver.get());

    _measureProgress->reset(cfg->durationInf ? 0 : cfg->durationSecs(), cfg->fileName);

    updateControls();
}

void PlotWindow::stopCapture()
{
    if (!_camera) return;
    auto thread = dynamic_cast<QThread*>(_camera.get());
    if (!thread) {
        qWarning() << LOG_ID << "Current camera is not thread based, nothing to stop";
        return;
    }
    Ori::Gui::PopupMessage::affirm(tr("Stopping..."), 0);
    QApplication::processEvents();
    thread->requestInterruption();
    if (!thread->wait(5000))
        qCritical() << LOG_ID << "Can not stop camera thread in timeout";
    else qDebug() << LOG_ID << "Camera thread stopped";
    Ori::Gui::PopupMessage::cancel();
}

void PlotWindow::captureStopped()
{
    showFps(0, 0);
}

void PlotWindow::statsReceived(const CameraStats &stats)
{
    showFps(stats.fps, stats.hardFps);
    if (stats.measureTime >= 0)
        _measureProgress->setElapsed(stats.measureTime);
}

void PlotWindow::dataReady()
{
    _statusBar->setVisible(STATUS_NO_DATA, _tableIntf->resultInvalid() && !_actionRawView->isChecked());
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
    stopCapture();
    _camera.reset((Camera*)cam);
    _tableIntf->setRows(_camera->dataRows());
    processImage();
}

void PlotWindow::openImage(const QString& fileName)
{
    if (_camera)
        stopCapture();
    _camera.reset((Camera*)new StillImageCamera(_plotIntf, _tableIntf, fileName));
    _tableIntf->setRows(_camera->dataRows());
    processImage();
}

void PlotWindow::processImage()
{
    auto cam = dynamic_cast<StillImageCamera*>(_camera.get());
    if (!cam) {
        qWarning() << LOG_ID << "Current camera is not StillImageCamera";
        return;
    }
    _plot->stopEditRoi(false);
    _plotIntf->cleanResult();
    _tableIntf->cleanResult();
    if (!cam->isDemoMode())
        _mru->append(cam->fileName());
    _camera->setRawView(_actionRawView->isChecked(), false);
    _camera->startCapture();
    // do showCamConfig() after capture(), when image is already loaded and its size gets known
    showCamConfig(false);
    updateHardConfgPanel();
    updateControls();
    _plot->zoomAuto(false);
    dataReady();
    showFps(0, 0);
}

void PlotWindow::editCamConfig(int pageId)
{
    const PixelScale prevScale = _camera->pixelScale();
    if (!_camera->editConfig(pageId))
        return;
    configChanged();
    if (_camera->pixelScale() != prevScale) {
        if (dynamic_cast<VirtualDemoCamera*>(_camera.get())) {
            _plot->zoomAuto(false);
        }
        else if (dynamic_cast<WelcomeCamera*>(_camera.get())) {
            _plot->zoomAuto(false);
            dataReady();
        }
        // StillImageCamera will reprocess image, nothing to do
    }
}

void PlotWindow::configChanged()
{
    if (dynamic_cast<StillImageCamera*>(_camera.get())) {
        processImage();
        return;
    }
    emit camConfigChanged();
    showCamConfig(true);
}

void PlotWindow::roiEdited()
{
    _camera->setAperture(_plot->roi());
    configChanged();
}

void PlotWindow::toggleRoi()
{
    bool on = _actionUseRoi->isChecked();
    if (!on && _plot->isRoiEditing())
        _plot->stopEditRoi(false);
    _camera->toggleAperture(on);
    configChanged();
}

void PlotWindow::activateCamWelcome()
{
    stopCapture();

    auto imgCam = dynamic_cast<StillImageCamera*>(_camera.get());
    if (imgCam) _prevImage = imgCam->fileName();

    _camera.reset((Camera*)new WelcomeCamera(_plotIntf, _tableIntf));
    _tableIntf->setRows(_camera->dataRows());
    showCamConfig(false);
    updateHardConfgPanel();
    showFps(0, 0);
    _camera->startCapture();
    updateControls();
    _plot->zoomAuto(false);
    dataReady();
}

void PlotWindow::activateCamImage()
{
    stopCapture();

    if (_prevImage.isEmpty())
        openImageDlg();
    else openImage(_prevImage);
}

void PlotWindow::activateCamDemo()
{
    if (dynamic_cast<VirtualDemoCamera*>(_camera.get())) return;

    auto imgCam = dynamic_cast<StillImageCamera*>(_camera.get());
    if (imgCam) _prevImage = imgCam->fileName();

    stopCapture();

    _plot->stopEditRoi(false);
    _plotIntf->cleanResult();
    _tableIntf->cleanResult();
    auto cam = new VirtualDemoCamera(_plotIntf, _tableIntf, this);
    connect(cam, &VirtualDemoCamera::ready, this, &PlotWindow::dataReady);
    connect(cam, &VirtualDemoCamera::stats, this, &PlotWindow::statsReceived);
    connect(cam, &VirtualDemoCamera::finished, this, &PlotWindow::captureStopped);
    cam->resultRowsChanged = [this]{ _tableIntf->setRows(_camera->dataRows()); };
    cam->setRawView(_actionRawView->isChecked(), false);
    _camera.reset((Camera*)cam);
    _tableIntf->setRows(_camera->dataRows());
    updateHardConfgPanel();
    showCamConfig(false);
    _plot->zoomAuto(false);
    _camera->startCapture();
    updateControls();
}

#ifdef WITH_IDS
void PlotWindow::activateCamIds()
{
    auto action = qobject_cast<QAction*>(sender());
    if (!action) return;

    auto camId = action->data();
    qDebug() << LOG_ID << "Open camera with id" << camId;

    auto imgCam = dynamic_cast<StillImageCamera*>(_camera.get());
    if (imgCam) _prevImage = imgCam->fileName();

    stopCapture();
    _camera.reset(nullptr);

    _plot->stopEditRoi(false);
    _plotIntf->cleanResult();
    _tableIntf->cleanResult();
    auto cam = new IdsCamera(camId, _plotIntf, _tableIntf, this);
    connect(cam, &IdsCamera::ready, this, &PlotWindow::dataReady);
    connect(cam, &IdsCamera::stats, this, &PlotWindow::statsReceived);
    connect(cam, &IdsCamera::finished, this, &PlotWindow::captureStopped);
    connect(cam, &IdsCamera::error, this, [this, cam](const QString& err){
        if (_saver)
            toggleMeasure(true);
        cam->stopCapture();
        updateControls();
        Ori::Dlg::error(err);
    });
    cam->resultRowsChanged = [this]{ _tableIntf->setRows(_camera->dataRows()); };
    cam->setRawView(_actionRawView->isChecked(), false);
    _camera.reset((Camera*)cam);
    _tableIntf->setRows(_camera->dataRows());
    updateHardConfgPanel();
    showCamConfig(false);
    _plot->zoomAuto(false);
    _camera->startCapture();
    if (!_camera->isCapturing()) {
        dataReady();
    }
    updateControls();
}
#endif

void PlotWindow::toggleResultsPanel()
{
    _resultsDock->setVisible(!_resultsDock->isVisible());
}

void PlotWindow::toggleHardConfig()
{
    _hardConfigDock->setVisible(!_hardConfigDock->isVisible());
    updateHardConfgPanel();
}

void PlotWindow::updateHardConfgPanel()
{
    if (!_hardConfigDock->isVisible())
        return;
    auto panel = _camera->hardConfgPanel(this);
    if (!panel) {
        if (_camConfigPanel != _stubConfigPanel) {
            if (_camConfigPanel)
                _camConfigPanel->deleteLater();
            _camConfigPanel = _stubConfigPanel;
            _hardConfigDock->setWidget(_camConfigPanel);
        }
        return;
    }
    if (panel != _camConfigPanel) {
        if (_camConfigPanel && _camConfigPanel != _stubConfigPanel)
            _camConfigPanel->deleteLater();
        _camConfigPanel = panel;
        _hardConfigDock->setWidget(_camConfigPanel);
    }
}

void PlotWindow::updateColorMapMenu()
{
    _colorMapMenu->clear();
    for (const auto& map : AppSettings::instance().colorMaps())
    {
        if (map.name.isEmpty()) {
            _colorMapMenu->addSeparator();
            continue;
        }
        QString name = map.name;
        if (!map.descr.isEmpty())
            name = QStringLiteral("%1 - %2").arg(name, map.descr);
        auto action = _colorMapMenu->addAction(name);
        action->setCheckable(true);
        action->setChecked(map.isCurrent);
        action->setDisabled(!map.isExists);
        connect(action, &QAction::triggered, this, [this, map]{
            AppSettings::instance().setCurrentColorMap(map.name);
            _plot->setColorMap(map.file, true);
        });
        _colorMapActions->addAction(action);
    }
    _colorMapMenu->addSeparator();
    _colorMapMenu->addAction(_actionLoadColorMap);
    _colorMapMenu->addAction(_actionCleanColorMaps);
}

void PlotWindow::selectColorMapFile()
{
    QString fileName = AppSettings::instance().selectColorMapFile();
    if (!fileName.isEmpty()) {
        AppSettings::instance().setCurrentColorMap(fileName);
        _plot->setColorMap(fileName, true);
    }
}

void PlotWindow::toggleRawView()
{
    auto rawView = _actionRawView->isChecked();
    _camera->setRawView(rawView, true);
    _plot->setRawView(rawView, true);

    if (auto imgCam = dynamic_cast<StillImageCamera*>(_camera.get()); imgCam)
        processImage();
}

void PlotWindow::toggleCrosshairsEditing()
{
    _plot->toggleCrosshairsEditing();
    _actionCrosshairsShow->setChecked(_plot->isCrosshairsVisible());
}

void PlotWindow::toggleCrosshairsVisbility()
{
    _plot->toggleCrosshairsVisbility();
    _actionCrosshairsEdit->setChecked(_plot->isCrosshairsEditing());
}

void PlotWindow::setCamCustomName()
{
    Ori::Settings s;
    s.beginGroup(INI_GROUP_CAM_NAMES);
    Ori::Dlg::InputTextOptions opts{
        .label = tr("Custom name for\n%1").arg(_camera->descr()),
        .value = s.value(_camera->customId()).toString(),
        .onHelp = []{ HelpSystem::topic("cam_name"); },
        .maxLength = 30,
    };
    if (!Ori::Dlg::inputText(opts))
        return;
    if (opts.value.isEmpty())
        s.settings()->remove(_camera->customId());
    else
        s.setValue(_camera->customId(), opts.value);
    fillCamSelector();
    showSelectedCamera();
}

void PlotWindow::devResizeWindow()
{
    auto edW = Ori::Gui::spinBox(100, 5000, width());
    auto edH = Ori::Gui::spinBox(100, 5000, height());
    QWidget w;
    auto layout = new QFormLayout(&w);
    layout->addRow("Width", edW);
    layout->addRow("Height", edH);
    if (Ori::Dlg::Dialog(&w, false).exec())
        resize(edW->value(), edH->value());
}

void PlotWindow::devResizeDock(QDockWidget *dock)
{
    std::shared_ptr<QSpinBox> ed(Ori::Gui::spinBox(20, 1000, dock->width()));
    if (Ori::Dlg::showDialogWithPromptH("Width", ed.get()))
        dock->resize(ed->value(), dock->height());
}

void PlotWindow::resultsTableDoubleClicked(QTableWidgetItem *item)
{
    if (_actionSetupPowerMeter->isVisible() && _actionSetupPowerMeter->isEnabled() && _tableIntf->isPowerRow(item))
        _actionSetupPowerMeter->trigger();
}
