#include "PlotWindow.h"

#include "app/AppSettings.h"
#include "app/HelpSystem.h"
#ifdef WITH_IDS
    #include "cameras/IdsCamera.h"
    #include "cameras/IdsCameraConfig.h"
#endif
#include "cameras/HardConfigPanel.h"
#include "cameras/MeasureSaver.h"
#include "cameras/StillImageCamera.h"
#include "cameras/VirtualDemoCamera.h"
#include "cameras/VirtualImageCamera.h"
#include "cameras/WelcomeCamera.h"
#include "helpers/OriDialogs.h"
#include "plot/PlotExport.h"
#include "widgets/DataTable.h"
#include "widgets/Plot.h"
#include "widgets/PlotIntf.h"
#include "widgets/ProfilesView.h"
#include "widgets/StabilityView.h"
#include "widgets/StabilityIntf.h"
#include "widgets/TableIntf.h"

#include "helpers/OriDialogs.h"
#include "helpers/OriWidgets.h"
#include "tools/OriMruList.h"
#include "tools/OriSettings.h"
#include "widgets/OriFlatToolBar.h"
#include "widgets/OriLabels.h"
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
    STATUS_SEPARATOR_4,
    STATUS_MOUSE_POS,

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
    setAcceptDrops(true);

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

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, &PlotWindow::updateThemeColors);
#endif
    setThemeColors();

    restoreState();

    _plot->setFocus();

    QTimer::singleShot(0, this, [this]{
        // dock still is not visible when asking in resoreState
        _actionResultsPanel->setChecked(_resultsDock->isVisible());
        _actionHardConfig->setChecked(_hardConfigDock->isVisible());
        _actionProfilesView->setChecked(_profilesDock->isVisible());
        _actionStabilityView->setChecked(_stabilityDock->isVisible());

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
    delete _stabilIntf;

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
    _plot->setBeamInfoVisible(showBeamInfo, false);
    _plot->restoreState(s.settings());
    _actionCrosshairsShow->setChecked(_plot->isCrosshairsVisible());
    _profilesView->restoreState(s.settings());
    _stabilityView->restoreState(s.settings());
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
        _actionCamDemoRender = A_("Demo (render)", this, &PlotWindow::activateCamDemoRender, ":/toolbar/bug");
        _actionCamDemoImage = A_("Demo (image)", this, &PlotWindow::activateCamDemoImage, ":/toolbar/bug");
    }
    _actionCamImage = A_("Image", this, &PlotWindow::activateCamImage, ":/toolbar/camera");
    _actionRefreshCams = A_("Refresh", this, &PlotWindow::fillCamSelector);
    _camSelectMenu = new QMenu(tr("Active Camera"), this);

    auto actnNew = A_(tr("New Window"), this, &PlotWindow::newWindow, ":/toolbar/new", QKeySequence::New);
    _actionOpenImg = A_(tr("Open Image..."), this, &PlotWindow::openImageDlg, ":/toolbar/open_img", QKeySequence::Open);
    _actionSaveRaw = A_(tr("Export Raw Image..."), this, [this]{ _camera->requestRawImg(this); }, ":/toolbar/save_raw", QKeySequence("F6"));
    auto actnSaveImg = A_(tr("Export Plot Image..."), this, [this]{ _plot->exportImageDlg(); }, ":/toolbar/save_img", QKeySequence("F7"));
    auto actnClose = A_(tr("Exit"), this, &PlotWindow::close);
    auto actnPrefs = A_(tr("Preferences..."), this, [this]{ AppSettings::instance().edit(); }, ":/toolbar/options", QKeySequence("F11"));
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
    _actionProfilesView = A_(tr("Profiles View"), this, &PlotWindow::toggleProfilesView, ":/toolbar/profile");
    _actionProfilesView->setCheckable(true);
    _actionStabilityView = A_(tr("Stability View"), this, &PlotWindow::toggleStabGraphView);
    _actionStabilityView->setCheckable(true);
    auto menuView = M_(tr("View"), {
        _actionRawView, 0,
        _actionResultsPanel, _actionHardConfig, _actionProfilesView, _actionStabilityView, 0,
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
    _actionEditRoi = A_(tr("Edit ROI"), this, &PlotWindow::editRoi, ":/toolbar/roi_edit");
    _actionUseRoi = A_(tr("Use ROI"), this, &PlotWindow::toggleRoi, ":/toolbar/roi");
    _actionUseRoi->setCheckable(true);
    _actionUseMultiRoi = A_(tr("Use Multi-ROI"), this, &PlotWindow::toggleMultiRoi, ":/toolbar/roi_multi");
    _actionUseMultiRoi->setCheckable(true);
    _actionSetupPowerMeter = A_(tr("Power Meter..."), this, &PlotWindow::setupPowerMeter);
    _actionSetCamCustomName = A_(tr("Custom Name..."), this, &PlotWindow::setCamCustomName);
    _actionCamConfig = A_(tr("Settings..."), this, [this]{ PlotWindow::editCamConfig(-1); }, ":/toolbar/settings");
    menuBar()->addMenu(M_(tr("Camera"), {
        _actionMeasure, 0,
        _actionUseRoi, _actionUseMultiRoi, _actionEditRoi, 0,
        _actionSetupPowerMeter, _actionSetCamCustomName, _actionCamConfig,
    }));

    _actionCrosshairsShow = A_(tr("Show Crosshairs"), this, &PlotWindow::toggleCrosshairsVisbility, ":/toolbar/crosshair");
    _actionCrosshairsShow->setCheckable(true);
    _actionCrosshairsEdit = A_(tr("Edit Crosshairs"), this, &PlotWindow::toggleCrosshairsEditing, ":/toolbar/crosshair_edit");
    _actionCrosshairsEdit->setCheckable(true);
    _actionClearCrosshairs = A_(tr("Clear Crosshairs"), _plot, &Plot::clearCrosshairs, ":/toolbar/trash");
    _actionLoadCrosshairs = A_(tr("Load From File..."), _plot, qOverload<>(&Plot::loadCrosshairs), ":/toolbar/open");
    _actionSaveCrosshairs = A_(tr("Save To File..."), _plot, &Plot::saveCrosshairs, ":/toolbar/save");
    auto menuOverlays = M_(tr("Overlays"), {
        _actionCrosshairsShow, 0, _actionCrosshairsEdit, _actionClearCrosshairs,
        0, _actionLoadCrosshairs, _actionSaveCrosshairs,
    });
    new Ori::Widgets::MruMenuPart(_plot->mruCrosshairs(), menuOverlays, _actionLoadCrosshairs, this);
    menuBar()->addMenu(menuOverlays);

    auto help = HelpSystem::instance();
    auto actnHelp = A_(tr("Help"), help, &HelpSystem::showContent, ":/ori_images/help", QKeySequence::HelpContents);
    auto actnHome = A_(tr("Visit Homepage"), help, &HelpSystem::visitHomePage, ":/toolbar/home");
    auto actnBug = A_(tr("Send Bug Report"), help, &HelpSystem::sendBugReport, ":/toolbar/bug");
    auto actnUpdates = A_(tr("Check For Updates"), help, &HelpSystem::checkForUpdates);
    auto actnAbout = A_(tr("About..."), help, &HelpSystem::showAbout);
    auto menuHelp = M_(tr("Help"), {
        actnHelp, actnHome, actnBug, actnUpdates, 0, actnAbout
    });
    menuBar()->addMenu(menuHelp);
#undef M_
#undef A_
}

void PlotWindow::fillCamSelector()
{
    _camSelectMenu->clear();
    if (AppSettings::instance().isDevMode) {
        _camSelectMenu->addAction(_actionCamWelcome);
        _camSelectMenu->addSeparator();
    }
    if (AppSettings::instance().isDevMode) {
        _camSelectMenu->addAction(_actionCamDemoRender);
        _camSelectMenu->addAction(_actionCamDemoImage);
        _camSelectMenu->addSeparator();
    }
    _camSelectMenu->addAction(_actionCamImage);
    _camSelectMenu->addSeparator();

#ifdef WITH_IDS
    Ori::Settings s;
    s.beginGroup(INI_GROUP_CAM_NAMES);
    _camCustomNames.clear();
    if (auto err = IdsCamera::libError(); !err.isEmpty()) {
        _camSelectMenu->addAction(QIcon(":/toolbar/error"), tr("IDS Camera"), this, [err]{
            Ori::Dlg::error(err);
        });
    } else {
        foreach (const auto& cam, IdsCamera::getCameras()) {
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
    _camSelectMenu->addSeparator();
#endif

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
    tb->addAction(_actionProfilesView);
    tb->addSeparator();
    tb->addAction(_actionUseRoi);
    tb->addAction(_actionUseMultiRoi);
    tb->addAction(_actionEditRoi);
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
    _statusBar->setText(STATUS_SEPARATOR_1, "|");
    _statusBar->setText(STATUS_SEPARATOR_2, "|");
    _statusBar->setText(STATUS_SEPARATOR_3, "|");
    _statusBar->setText(STATUS_SEPARATOR_4, "|");
    _statusBar->setMargin(STATUS_SEPARATOR_1, 0, 0);
    _statusBar->setMargin(STATUS_SEPARATOR_2, 0, 0);
    _statusBar->setMargin(STATUS_SEPARATOR_3, 0, 0);
    _statusBar->setMargin(STATUS_SEPARATOR_4, 0, 0);
    _statusBar->setText(STATUS_MOUSE_POS, "");
    _statusBar->setHint(STATUS_MOUSE_POS, tr("Mouse coordinates"));
    _statusBar->setMargin(STATUS_ROI_ICON, 6, 0);
    _statusBar->setMargin(STATUS_ROI, 0, 6);
    _statusBar->setDblClick(STATUS_ROI, [this]{ editRoiCfg(); });
    _statusBar->setDblClick(STATUS_ROI_ICON, [this]{ editRoiCfg(); });
    _statusBar->setIcon(STATUS_NO_DATA, ":/toolbar/error");
    _statusBar->setHint(STATUS_NO_DATA, tr("No data to process.\n"
        "All pixels in the region are below noise threshold"));
    _statusBar->setIcon(STATUS_BGND, ":/toolbar/exclame");
    _statusBar->setHint(STATUS_BGND, tr("Background subtraction disabled.\n"
        "The measurement is not compliant with the ISO standard."));
    _statusBar->setDblClick(STATUS_BGND, [this]{ editCamConfig(Camera::cfgBgnd); });
    auto verLabel = _statusBar->addVersionLabel();
    connect(verLabel, &Ori::Widgets::Label::doubleClicked, this, []{ HelpSystem::instance()->showAbout(); });

    _measureProgress = new MeasureProgressBar;
    _measureProgress->setVisible(false);
    _statusBar->addWidget(_measureProgress);

    setStatusBar(_statusBar);
}

void PlotWindow::createDockPanel()
{
    _table = new DataTable;
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

    _profilesView = new ProfilesView(_plot->plotIntf());

    _profilesDock = new QDockWidget(tr("Profiles"));
    _profilesDock->setObjectName("DockProfiles");
    _profilesDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    _profilesDock->setFeatures(QDockWidget::DockWidgetMovable);
    _profilesDock->setVisible(false);
    _profilesDock->setWidget(_profilesView);
    addDockWidget(Qt::BottomDockWidgetArea, _profilesDock);
    
    _stabilityView = new StabilityView();
    _stabilIntf = new StabilityIntf(_stabilityView);
    
    _stabilityDock = new QDockWidget(tr("Stability"));
    _stabilityDock->setObjectName("DockStability");
    _stabilityDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    _stabilityDock->setFeatures(QDockWidget::DockWidgetMovable);
    _stabilityDock->setVisible(false);
    _stabilityDock->setWidget(_stabilityView);
    addDockWidget(Qt::BottomDockWidgetArea, _stabilityDock);
}

void PlotWindow::createPlot()
{
    _plot = new Plot;
    _plotIntf = _plot->plotIntf();
    connect(_plot, &Plot::roiEdited, this, &PlotWindow::roiEdited);
    connect(_plot, &Plot::crosshairsEdited, this, &PlotWindow::crosshairsEdited);
    connect(_plot, &Plot::crosshairsLoaded, this, &PlotWindow::crosshairsLoaded);
    connect(_plot, &Plot::mousePositionChanged, this, [this](double x, double y, double c) {
        if (!_camera) return;
        const PixelScale& scale = _camera->pixelScale();
        QString coordText = QStringLiteral("%1 @ (%2 × %3)").arg(_camera->formatBrightness(c), scale.format(x), scale.format(y));
        _statusBar->setText(STATUS_MOUSE_POS, coordText);
    });
    _plot->augmentCrosshairLoadSave(
        [this](QJsonObject &root){ loadCrosshairsMore(root); },
        [this](QJsonObject &root){ saveCrosshairsMore(root); }
    );
}

void PlotWindow::closeEvent(QCloseEvent* ce)
{
    QMainWindow::closeEvent(ce);

    if (_saver) {
        if (!Ori::Dlg::yes(tr("Measurements still in progress.\n\nInterrupt?"))) {
            ce->ignore();
            return;
        }
        stopMeasure(true);
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

#include <QMimeData>
void PlotWindow::dragEnterEvent(QDragEnterEvent* e)
{
    auto urls = e->mimeData()->urls();
    for (const auto &url : std::as_const(urls)) {
        if (url.scheme() == QStringLiteral("file")) {
            QString ext = QFileInfo(url.path()).suffix().toLower();
            if (ext == "json" || CameraCommons::supportedImageExts().contains(ext)) {
                e->acceptProposedAction();
                return;
            }
        }
    }
}

void PlotWindow::dropEvent(QDropEvent* e)
{
    auto urls = e->mimeData()->urls();
    for (const auto &url : std::as_const(urls)) {
        QString fileName = url.path();
        if (fileName.startsWith('/'))
            fileName = fileName.right(fileName.length()-1);
        QString ext = QFileInfo(fileName).suffix().toLower();
        if (ext == "json") {
            _plot->loadCrosshairs(fileName);
            e->acceptProposedAction();
            return;
        }
        if (CameraCommons::supportedImageExts().contains(ext)) {
            openImage(fileName);
            e->acceptProposedAction();
            return;
        }
    }
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
        _statusBar->setStyleSheet(STATUS_FPS, {});
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
    _actionEditRoi->setVisible(_camera->config().roiMode == ROI_SINGLE);
    showSelectedCamera();

    _statusBar->setText(STATUS_CAMERA, _camera->name(), _camera->descr());
    _statusBar->setText(STATUS_RESOLUTION, _camera->resolutionStr());

    const auto &c = _camera->config();
    _actionUseRoi->setChecked(c.roiMode == ROI_SINGLE);
    _actionUseMultiRoi->setChecked(c.roiMode == ROI_MULTI);
    // TODO: adapt for ROI_MULTI
    _actionZoomRoi->setVisible(c.roiMode == ROI_SINGLE);
    _statusBar->setVisible(STATUS_ROI_ICON, c.roiMode != ROI_NONE);
    _statusBar->setVisible(STATUS_ROI, c.roiMode == ROI_SINGLE);
    if (c.roiMode == ROI_SINGLE) {
        // TODO: remove old logic
        // this is from the old version where rois where stored in pixels
        // in relative coordibates rois can't be invalid
        // if we do roi.fix() before setting them to camera config
        bool valid = _camera->isRoiValid();
        QString hint = valid ? tr("Region of interest") : tr("Region is not valid");
        QString icon = valid ? ":/toolbar/roi" : ":/toolbar/roi_warn";
        _statusBar->setIcon(STATUS_ROI_ICON, icon);
        _statusBar->setHint(STATUS_ROI_ICON, hint);
        _statusBar->setText(STATUS_ROI, c.roi.sizeStr(_camera->width(), _camera->height()));
        _statusBar->setHint(STATUS_ROI, hint);
    } else if (c.roiMode == ROI_MULTI) {
        _statusBar->setIcon(STATUS_ROI_ICON, ":/toolbar/roi_multi");
        _statusBar->setHint(STATUS_ROI_ICON, tr("Several ROIs are used"));
    }

    _statusBar->setVisible(STATUS_BGND, !c.bgnd.on);

    auto s = _camera->pixelScale();
    _plot->setColorMap(c.plot.colorMap, false);
    _plot->setImageSize(_camera->width(), _camera->height(), s);
    _plot->setRoi(c.roi);
    _plot->setRois(c.rois);
    _plot->setRoiMode(c.roiMode);
    _tableIntf->setRows(_camera->tableRows());
    _tableIntf->setScale(s);
    _plotIntf->setScale(s);
    _profilesView->setConfig(s, _camera->config().mavg);
    _stabilityView->setConfig(s);

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
    QTimer::singleShot(500, this, [this]{
        setThemeColors();
        _plot->setThemeColors(PlotHelpers::SYSTEM, true);
        _profilesView->setThemeColors(PlotHelpers::SYSTEM, true);
        _stabilityView->setThemeColors(PlotHelpers::SYSTEM, true);
    });
}

void PlotWindow::updateZoom()
{
    if (!_keepZoom)
        _plot->zoomAuto(false);
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
    _actionUseMultiRoi->setDisabled(started || !opened);
    _actionOpenImg->setDisabled(started);
    _actionRawView->setDisabled(started || !opened);
    _actionMeasure->setText(started ? tr("Stop Measurements") : tr("Start Measurements"));
    _actionMeasure->setIcon(QIcon(started ? ":/toolbar/stop" : ":/toolbar/start"));
    _actionMeasure->setDisabled(!opened);
    _camConfigPanel->setReadOnly(started || !opened);
    _actionSetupPowerMeter->setDisabled(started || !opened);
    _actionCrosshairsShow->setDisabled(started || !opened);
    _actionCrosshairsEdit->setDisabled(started || !opened);
    _actionClearCrosshairs->setDisabled(started || !opened);
    _actionLoadCrosshairs->setDisabled(started || !opened);
    _actionSaveCrosshairs->setDisabled(started || !opened);
}

void PlotWindow::toggleMeasure()
{
    if (_saver)
        stopMeasure(false);
    else
        startMeasure();
}

void PlotWindow::startMeasure()
{
    auto cam = _camera.get();
    if (!cam) return;

    if (!cam->isCapturing()) {
        Ori::Gui::PopupMessage::warning(tr("Camera is not opened"));
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
        stopMeasure(true);
        Ori::Gui::PopupMessage::affirm(tr("<b>Measurements finished<b>"), 0);
    });
    connect(saver, &MeasureSaver::failed, this, [this](const QString &error){
        stopMeasure(true);
        Ori::Gui::PopupMessage::error(tr("<b>Measurements failed</b><p>") + QString(error).replace("\n", "<br>"), 0);
    });
    _saver.reset(saver);
    Ori::Gui::PopupMessage::cancel();
    cam->startMeasure(_saver.get());

    _measureProgress->reset(cfg->durationInf ? 0 : cfg->durationSecs(), cfg->fileName);

    updateControls();
}

void PlotWindow::stopMeasure(bool force)
{
    auto cam = _camera.get();
    if (!cam) return;

    if (!_saver) return;

    if (!force) {
        if (!Ori::Dlg::yes(tr("Interrupt measurements?")))
            return;
        // could be finished while dialog opened
        if (!_saver)
            return;
    }

    cam->stopMeasure();
    // Process the last MeasureEvent, if it is
    qApp->processEvents();
    _saver.reset(nullptr);
    _measureProgress->setVisible(false);
    updateControls();
    return;
}

void PlotWindow::stopCapture()
{
    if (!_camera) return;
    auto thread = dynamic_cast<QThread*>(_camera.get());
    if (!thread) {
        //qWarning() << LOG_ID << "Current camera is not thread based, nothing to stop";
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
    if (_profilesDock->isVisible())
        _profilesView->showResult();
    if (_stabilityDock->isVisible())
        _stabilityView->showResult();
}

void PlotWindow::openImageDlg()
{
    auto cam = new StillImageCamera(_plotIntf, _tableIntf, _stabilIntf);
    if (cam->fileName().isEmpty()) {
        delete cam;
        return;
    }
    stopCapture();
    _camera.reset((Camera*)cam);
    processImage();
}

void PlotWindow::openImage(const QString& fileName)
{
    if (_camera)
        stopCapture();
    _camera.reset((Camera*)new StillImageCamera(_plotIntf, _tableIntf, _stabilIntf, fileName));
    processImage();
}

void PlotWindow::processImage()
{
    auto cam = dynamic_cast<StillImageCamera*>(_camera.get());
    if (!cam) {
        qWarning() << LOG_ID << "Current camera is not StillImageCamera";
        return;
    }
    cleanResults();
    if (!cam->isDemoMode())
        _mru->append(cam->fileName());
    _camera->setRawView(_actionRawView->isChecked(), false);
    _camera->startCapture();
    // do showCamConfig() after capture(), when image is already loaded and its size gets known
    showCamConfig(false);
    updateHardConfgPanel();
    updateControls();
    updateZoom();
    dataReady();
    showFps(0, 0);
}

void PlotWindow::cleanResults()
{
    _plot->stopEditRoi(false);
    _plotIntf->cleanResult();
    _tableIntf->cleanResult();
    _profilesView->cleanResult();
    _stabilityView->cleanResult();
}

void PlotWindow::editCamConfig(int pageId)
{
    const PixelScale prevScale = _camera->pixelScale();
    if (!_camera->editConfig(pageId))
        return;
    doCamConfigChanged();
    if (_camera->pixelScale() != prevScale) {
        if (dynamic_cast<StillImageCamera*>(_camera.get())) {
            // The image will reprocessed as if it's a new one, nothing to do
            return;
        }
        updateZoom();
        if (dynamic_cast<WelcomeCamera*>(_camera.get())) {
            dataReady();
        }
    }
}

void PlotWindow::editRoi()
{
    if (_camera->config().roiMode == ROI_MULTI)
        editRoisSize();
    if (_camera->config().roiMode == ROI_SINGLE)
        _plot->startEditRoi();
}

void PlotWindow::editRoiCfg()
{
    if (_camera->config().roiMode == ROI_MULTI)
        editRoisSize();
    if (_camera->config().roiMode == ROI_SINGLE)
        editCamConfig(Camera::cfgRoi);
}

void PlotWindow::editRoisSize()
{
    // For now, roi sizes are taken from crosshairs
    return;

    if (_camera->editRoisSize())
        doCamConfigChanged();
}

void PlotWindow::setupPowerMeter()
{
    if (_camera->setupPowerMeter())
        doCamConfigChanged();
}

void PlotWindow::doCamConfigChanged()
{
    if (dynamic_cast<StillImageCamera*>(_camera.get())) {
        emit camConfigChanged();
        processImage();
        return;
    }
    emit camConfigChanged();
    showCamConfig(true);
}

void PlotWindow::roiEdited()
{
    _camera->setRoi(_plot->roi());
    doCamConfigChanged();
}

void PlotWindow::crosshairsEdited()
{
    _camera->setRois(_plot->rois());
    _keepZoom = true;
    doCamConfigChanged();
    _keepZoom = false;
}

void PlotWindow::crosshairsLoaded()
{
    _camera->setRois(_plot->rois());
    doCamConfigChanged();
#ifdef WITH_IDS
    if (auto cam = dynamic_cast<IdsCamera*>(_camera.get()); cam) {
        cam->saveConfig(true);
        if (_camConfigPanel) {
            qDebug() << "Try update hard config panel";
            QApplication::postEvent(_camConfigPanel, new UpdateSettingsEvent());
        }
    }
#endif
}

void PlotWindow::toggleRoi()
{
    bool on = _actionUseRoi->isChecked();
    if (!on && _plot->isRoiEditing())
        _plot->stopEditRoi(false);
    _camera->setRoiMode(on ? ROI_SINGLE : ROI_NONE);
    doCamConfigChanged();
}

void PlotWindow::toggleMultiRoi()
{
    bool on = _actionUseMultiRoi->isChecked();
    if (_plot->isRoiEditing())
        _plot->stopEditRoi(false);
    if (on) {
        if (_camera->config().rois.empty()) {
            //auto items = _plot->crosshairs();
            auto items = _plot->rois();
            if (items.empty()) {
                Ori::Gui::PopupMessage::hint(tr(
                    "Multiple ROIs are building around crosshairs.\n"
                    "Currently there are no crosshairs defined.\n"
                    "Use the command 'Overlays -> Edit Crosshairs' to add them."), 0);
            } else {
                _camera->setRois(items);
            }
        }
    }
    _camera->setRoiMode(on ? ROI_MULTI : ROI_NONE);
    doCamConfigChanged();
}

void PlotWindow::activateCamWelcome()
{
    qDebug() << LOG_ID << "Activate camera: Welcome";
    
    stopCapture();

    auto imgCam = dynamic_cast<StillImageCamera*>(_camera.get());
    if (imgCam) _prevImage = imgCam->fileName();

    _camera.reset((Camera*)new WelcomeCamera(_plotIntf, _tableIntf, _stabilIntf));
    showCamConfig(false);
    updateHardConfgPanel();
    showFps(0, 0);
    _camera->startCapture();
    updateControls();
    updateZoom();
    dataReady();
}

void PlotWindow::activateCamImage()
{
    qDebug() << LOG_ID << "Activate camera: Image";

    if (_prevImage.isEmpty())
        openImageDlg();
    else openImage(_prevImage);
}

void PlotWindow::activateCamDemoRender()
{
    qDebug() << LOG_ID << "Activate camera: Demo (render)";

    auto imgCam = dynamic_cast<StillImageCamera*>(_camera.get());
    if (imgCam) _prevImage = imgCam->fileName();

    stopCapture();

    cleanResults();
    auto cam = new VirtualDemoCamera(_plotIntf, _tableIntf, _stabilIntf, this);
    connect(cam, &VirtualDemoCamera::ready, this, &PlotWindow::dataReady);
    connect(cam, &VirtualDemoCamera::stats, this, &PlotWindow::statsReceived);
    connect(cam, &VirtualDemoCamera::finished, this, &PlotWindow::captureStopped);
    cam->setRawView(_actionRawView->isChecked(), false);
    _camera.reset((Camera*)cam);
    updateHardConfgPanel();
    showCamConfig(false);
    updateZoom();
    _camera->startCapture();
    updateControls();
}

void PlotWindow::activateCamDemoImage()
{
    qDebug() << LOG_ID << "Activate camera: Demo (image)";

    auto imgCam = dynamic_cast<StillImageCamera*>(_camera.get());
    if (imgCam) _prevImage = imgCam->fileName();

    stopCapture();

    cleanResults();
    auto cam = new VirtualImageCamera(_plotIntf, _tableIntf, _stabilIntf, this);
    connect(cam, &VirtualImageCamera::ready, this, &PlotWindow::dataReady);
    connect(cam, &VirtualImageCamera::stats, this, &PlotWindow::statsReceived);
    connect(cam, &VirtualImageCamera::finished, this, &PlotWindow::captureStopped);
    cam->setRawView(_actionRawView->isChecked(), false);
    _camera.reset((Camera*)cam);
    updateHardConfgPanel();
    showCamConfig(false);
    updateZoom();
    _camera->startCapture();
    updateControls();
}

#ifdef WITH_IDS
void PlotWindow::activateCamIds()
{
    auto action = qobject_cast<QAction*>(sender());
    if (!action) return;

    auto camId = action->data();
    qDebug().noquote().nospace() << LOG_ID << " Activate camera: IDS id=" << camId.toString();

    auto imgCam = dynamic_cast<StillImageCamera*>(_camera.get());
    if (imgCam) _prevImage = imgCam->fileName();

    stopCapture();
    _camera.reset(nullptr);

    cleanResults();
    auto cam = new IdsCamera(camId, _plotIntf, _tableIntf, _stabilIntf, this);
    connect(cam, &IdsCamera::ready, this, &PlotWindow::dataReady);
    connect(cam, &IdsCamera::stats, this, &PlotWindow::statsReceived);
    connect(cam, &IdsCamera::finished, this, &PlotWindow::captureStopped);
    connect(cam, &IdsCamera::error, this, [this, cam](const QString& err){
        stopMeasure(true);
        cam->stopCapture();
        updateControls();
        Ori::Dlg::error(err);
    });
    cam->setRawView(_actionRawView->isChecked(), false);
    _camera.reset((Camera*)cam);
    updateHardConfgPanel();
    showCamConfig(false);
    updateZoom();
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

void PlotWindow::toggleProfilesView()
{
    _profilesDock->setVisible(!_profilesDock->isVisible());
    if (_profilesDock->isVisible())
        _profilesView->showResult();
}

void PlotWindow::toggleStabGraphView()
{
    _stabilityDock->setVisible(!_stabilityDock->isVisible());
    if (_stabilityDock->isVisible())
        _stabilityView->showResult();
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

    QString currentColorMap = _camera ? _camera->config().plot.colorMap : "";
    if (currentColorMap.isEmpty())
        currentColorMap = AppSettings::defaultColorMap();

    auto colorMaps = AppSettings::instance().colorMaps();
    for (const auto& map : std::as_const(colorMaps))
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

        action->setChecked(map.file == currentColorMap);
        action->setDisabled(!map.isExists);

        connect(action, &QAction::triggered, this, [this, map]{
            if (_camera) {
                _camera->setColorMap(map.file);
                _plot->setColorMap(map.file, true);
            }
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
    if (!fileName.isEmpty() && _camera) {
        _camera->setColorMap(fileName);
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

void PlotWindow::saveCrosshairsMore(QJsonObject &root)
{
#ifdef WITH_IDS
    if (auto cam = dynamic_cast<IdsCamera*>(_camera.get()); cam)
        cam->idsConfig()->saveExpPresets(root);
#endif
}

void PlotWindow::loadCrosshairsMore(QJsonObject &root)
{
#ifdef WITH_IDS
    if (auto cam = dynamic_cast<IdsCamera*>(_camera.get()); cam)
        cam->idsConfig()->loadExpPresets(root);
#endif
}

void PlotWindow::settingsChanged(bool affectsCamera)
{
#ifdef WITH_IDS
    if (auto cam = dynamic_cast<IdsCamera*>(_camera.get()); cam)
        cam->requestExpWarning();
#endif
    if (affectsCamera)
        doCamConfigChanged();
}
