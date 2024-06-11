#include "PlotWindow.h"

#include "app/AppSettings.h"
#include "app/HelpSystem.h"
#ifdef WITH_IDS
    #include "cameras/IdsComfortCamera.h"
#endif
#include "cameras/HardConfigPanel.h"
#include "cameras/MeasureSaver.h"
#include "cameras/StillImageCamera.h"
#include "cameras/VirtualDemoCamera.h"
#include "cameras/WelcomeCamera.h"
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
#include <QDockWidget>
#include <QHeaderView>
#include <QLabel>
#include <QMenuBar>
#include <QProcess>
#include <QProgressBar>
#include <QStatusBar>
#include <QStyleHints>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QToolTip>
#include <QWindowStateChangeEvent>

#define LOG_ID "PlotWindow:"

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
    void reset(int duration)
    {
        setElapsed(0);
        setMaximum(duration);
        setVisible(true);
        setFormat("%p%");
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

private:
    int _secs;

    QString formatTooltip() const {
        int max = maximum();
        if (max == 0)
            return tr("Measurements<br>Elapsed: <b>%1</b>").arg(formatSecs(_secs));
        return tr("Measurements"
            "<br>Duration: <b>%1</b>"
            "<br>Elapsed: <b>%2</b>"
            "<br>Remaining: <b>%3</b>")
            .arg(formatSecs(max), formatSecs(_secs), formatSecs(max - _secs));
    }
};

//------------------------------------------------------------------------------
//                               PlotWindow
//------------------------------------------------------------------------------

PlotWindow::PlotWindow(QWidget *parent) : QMainWindow(parent)
{
    Ori::Gui::PopupMessage::setTarget(this);

    _mru = new Ori::MruFileList(this);
    connect(_mru, &Ori::MruFileList::clicked, this, &PlotWindow::openImage);

#ifdef WITH_IDS
    if (auto ids = IdsComfort::init(); ids) _ids.reset(ids);
#endif

    createMenuBar();
    createToolBar();
    createStatusBar();
    createDockPanel();
    createPlot();
    setCentralWidget(_plot);
    fillCamSelector();

    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, &PlotWindow::updateThemeColors);
    setThemeColors();

    restoreState();
    resize(1100, 600);

    _plot->setFocus();

    QTimer::singleShot(0, this, [this]{
        // This initializes all graph structs
        activateCamWelcome();

        if (!AppSettings::instance().isDevMode)
            openImage({});
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
    s.restoreWindowGeometry(this);
    s.restoreDockState(this);

    _mru->setMaxCount(20);
    _mru->load(s.settings());

    s.beginGroup("Plot");
    bool showBeamInfo = s.value("beamInfo", false).toBool();
    bool rawView = s.value("rawView", false).toBool();

    _actionBeamInfo->setChecked(showBeamInfo);
    _actionRawView->setChecked(rawView);
    _plot->setRawView(rawView, false);
    _plot->setColorMap(AppSettings::instance().currentColorMap(), false);
    _plot->setBeamInfoVisible(showBeamInfo, false);
}

void PlotWindow::storeState()
{
    Ori::Settings s;
    s.storeWindowGeometry(this);
    s.storeDockState(this);

    s.beginGroup("Plot");
    s.setValue("beamInfo", _actionBeamInfo->isChecked());
    s.setValue("rawView", _actionRawView->isChecked());
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
    _colorMapMenu = new QMenu(tr("Colour Map"));
    _actionLoadColorMap = A_(tr("Load From File..."), this, &PlotWindow::selectColorMapFile);
    _actionCleanColorMaps = A_(tr("Delete Invalid Items"), this, []{ AppSettings::instance().deleteInvalidColorMaps(); });
    connect(_colorMapMenu, &QMenu::aboutToShow, this, &PlotWindow::updateColorMapMenu);
    _colorMapActions = new QActionGroup(this);
    _colorMapActions->setExclusive(true);
    _actionHardConfig = A_(tr("Device Control"), this, &PlotWindow::toggleHardConfig, ":/toolbar/hardware");
    _actionHardConfig->setCheckable(true);
    auto menuView = M_(tr("View"), {
        _actionRawView, 0,
        _actionHardConfig, _actionBeamInfo, 0,
        _colorMapMenu, 0,
        _actionZoomFull, _actionZoomRoi,
    });
    connect(menuView, &QMenu::aboutToShow, this, &PlotWindow::updateViewMenu);
    menuBar()->addMenu(menuView);

    _actionMeasure = A_(tr("Start Measurements"), this, &PlotWindow::toggleMeasure, ":/toolbar/start", Qt::Key_F9);
    _actionEditRoi = A_(tr("Edit ROI"), this, [this]{ _plot->startEditRoi(); }, ":/toolbar/roi");
    _actionUseRoi = A_(tr("Use ROI"), this, &PlotWindow::toggleRoi, ":/toolbar/roi_rect");
    _actionUseRoi->setCheckable(true);
    _actionCamConfig = A_(tr("Settings..."), this, [this]{ PlotWindow::editCamConfig(-1); }, ":/toolbar/settings");
    menuBar()->addMenu(M_(tr("Camera"), {
        _actionMeasure, 0,
        _actionEditRoi, _actionUseRoi, 0,
        _actionCamConfig,
    }));

    auto m = menuBar()->addMenu(tr("Help"));
    auto help = HelpSystem::instance();
    m->addAction(QIcon(":/toolbar/home"), tr("Visit Homepage"), help, &HelpSystem::visitHomePage);
    m->addAction(QIcon(":/toolbar/bug"), tr("Send Bug Report"), help, &HelpSystem::sendBugReport);
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
    if (_ids)
        for (const auto& cam : _ids->getCameras()) {
            auto a = _camSelectMenu->addAction(QIcon(":/toolbar/camera"), cam.name, this, &PlotWindow::activateCamIds);
            a->setData(cam.id);
        }
#endif

    // TODO: do something if active camera disappeared

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
    addToolBar(tb);
    tb->setMovable(false);
    tb->addWidget(_buttonSelectCam);
    tb->addAction(_actionCamConfig);
    tb->addAction(_actionHardConfig);
    tb->addSeparator();
    tb->addAction(_actionRawView);
    tb->addSeparator();
    tb->addAction(_actionEditRoi);
    tb->addAction(_actionUseRoi);
    tb->addSeparator();
    _buttonMeasure = tb->addWidget(Ori::Gui::textToolButton(_actionMeasure));
    _buttonOpenImg = tb->addWidget(Ori::Gui::textToolButton(_actionOpenImg));
    tb->addSeparator();
    tb->addAction(_actionZoomFull);
    tb->addAction(_actionZoomRoi);
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
    _itemErrCount = makeItem(tr("Error frames"));

    auto dock = new QDockWidget(tr("Results"));
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dock->setFeatures(QDockWidget::DockWidgetMovable);
    dock->setWidget(_table);
    addDockWidget(Qt::LeftDockWidgetArea, dock);

    _stubConfigPanel = new StubHardConfigPanel(this);
    _camConfigPanel = _stubConfigPanel;
    _hardConfigDock = new QDockWidget(tr("Control"));
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

void PlotWindow::showFps(int fps)
{
    if (fps <= 0)
        _statusBar->setText(STATUS_FPS, QStringLiteral("FPS: NA"));
    else _statusBar->setText(STATUS_FPS, QStringLiteral("FPS: ") % QString::number(fps));
}

void PlotWindow::showCamConfig(bool replot)
{
    bool isImage = dynamic_cast<StillImageCamera*>(_camera.get());
    _buttonOpenImg->setVisible(isImage);
    _actionMeasure->setVisible(_camera->canMeasure());
    _buttonMeasure->setVisible(_camera->canMeasure());
    _buttonSelectCam->setText("  " + (isImage ? _actionCamImage->text() : _camera->name()));
    _actionSaveRaw->setEnabled(_camera->isCapturing());

    setWindowTitle(qApp->applicationName() +  " [" + _camera->name() + ']');
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
        _statusBar->setText(STATUS_ROI, c.roi.sizeStr());
        _statusBar->setHint(STATUS_ROI, hint);
    }

    _statusBar->setVisible(STATUS_BGND, !c.bgnd.on);

    auto s = _camera->pixelScale();
    _plot->setImageSize(_camera->width(), _camera->height(), s);
    if (c.roi.isZero())
        _plot->setRoi({false, 0, 0, _camera->width(), _camera->height()});
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

void PlotWindow::updateActions()
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
        _saver.reset(nullptr);
        _measureProgress->setVisible(false);
        _camConfigPanel->setReadOnly(false);
        updateActions();
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

    _camConfigPanel->setReadOnly(true);
    _measureProgress->reset(cfg->durationInf ? 0 : cfg->durationSecs());

    updateActions();
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
    showFps(0);
}

void PlotWindow::statsReceived(const CameraStats &stats)
{
    showFps(stats.fps);
    if (stats.measureTime >= 0)
        _measureProgress->setElapsed(stats.measureTime);
    _itemErrCount->setText(QStringLiteral(" %1 ").arg(stats.errorFrames));
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
    processImage();
}

void PlotWindow::openImage(const QString& fileName)
{
    if (_camera)
        stopCapture();
    _camera.reset((Camera*)new StillImageCamera(_plotIntf, _tableIntf, fileName));
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
    _itemRenderTime->setText(tr(" Load time "));
    _mru->append(cam->fileName());
    _camera->setRawView(_actionRawView->isChecked(), false);
    _camera->startCapture();
    // do showCamConfig() after capture(), when image is already loaded and its size gets known
    showCamConfig(false);
    updateHardConfgPanel();
    _plot->zoomAuto(false);
    dataReady();
    showFps(0);
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

    _itemRenderTime->setText(tr(" Render time "));
    _camera.reset((Camera*)new WelcomeCamera(_plotIntf, _tableIntf));
    showCamConfig(false);
    updateHardConfgPanel();
    showFps(0);
    _camera->startCapture();
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
    _itemRenderTime->setText(tr(" Render time "));
    auto cam = new VirtualDemoCamera(_plotIntf, _tableIntf, this);
    connect(cam, &VirtualDemoCamera::ready, this, &PlotWindow::dataReady);
    connect(cam, &VirtualDemoCamera::stats, this, &PlotWindow::statsReceived);
    connect(cam, &VirtualDemoCamera::finished, this, &PlotWindow::captureStopped);
    cam->setRawView(_actionRawView->isChecked(), false);
    _camera.reset((Camera*)cam);
    updateHardConfgPanel();
    showCamConfig(false);
    _plot->zoomAuto(false);
    _camera->startCapture();
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
    _itemRenderTime->setText(tr(" Acq. time "));
    auto cam = new IdsComfortCamera(camId, _plotIntf, _tableIntf, this);
    connect(cam, &IdsComfortCamera::ready, this, &PlotWindow::dataReady);
    connect(cam, &IdsComfortCamera::stats, this, &PlotWindow::statsReceived);
    connect(cam, &IdsComfortCamera::finished, this, &PlotWindow::captureStopped);
    connect(cam, &IdsComfortCamera::error, this, [this, cam](const QString& err){
        Ori::Dlg::error(err);
        if (_saver)
            toggleMeasure(true);
        cam->stopCapture();
    });
    cam->setRawView(_actionRawView->isChecked(), false);
    _camera.reset((Camera*)cam);
    updateHardConfgPanel();
    showCamConfig(false);
    _plot->zoomAuto(false);
    _camera->startCapture();
    if (!_camera->isCapturing()) {
        dataReady();
    }
    updateActions();
}
#endif

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

void PlotWindow::updateViewMenu()
{
    _actionHardConfig->setChecked(_hardConfigDock->isVisible());
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
