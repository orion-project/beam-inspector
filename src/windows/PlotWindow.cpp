#include "PlotWindow.h"

#include "app/HelpSystem.h"
#include "cameras/MeasureSaver.h"
#include "cameras/StillImageCamera.h"
#include "cameras/VirtualDemoCamera.h"
#include "cameras/WelcomeCamera.h"
#include "widgets/Plot.h"
#include "widgets/PlotIntf.h"
#include "widgets/TableIntf.h"

#include "helpers/OriDialogs.h"
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
#include <QToolButton>
#include <QWindowStateChangeEvent>

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

PlotWindow::PlotWindow(QWidget *parent) : QMainWindow(parent)
{
    _mru = new Ori::MruFileList(this);
    connect(_mru, &Ori::MruFileList::clicked, this, &PlotWindow::openImage);

    createMenuBar();
    createToolBar();
    createStatusBar();
    createDockPanel();
    createPlot();
    setCentralWidget(_plot);

    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, &PlotWindow::updateThemeColors);
    setThemeColors();

    restoreState();
    resize(800, 600);

    _plot->setFocus();

    QTimer::singleShot(0, this, &PlotWindow::activateCamWelcome);
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
    _actionCamWelcome = A_("Welcome", this, &PlotWindow::activateCamWelcome, ":/toolbar/cam_select");
    _actionCamImage = A_("Image", this, &PlotWindow::activateCamImage, ":/toolbar/cam_select");
    _actionCamDemo = A_("Demo", this, &PlotWindow::activateCamDemo, ":/toolbar/cam_select");
    _camSelectMenu = M_(tr("Active Camera"), {_actionCamWelcome, _actionCamImage, _actionCamDemo});

    auto actnNew = A_(tr("New Window"), this, &PlotWindow::newWindow, ":/toolbar/new", QKeySequence::New);
    _actionOpenImg = A_(tr("Open Image..."), this, &PlotWindow::openImageDlg, ":/toolbar/open_img", QKeySequence::Open);
    auto actnExport = A_(tr("Export Image..."), this, [this]{ _plot->exportImageDlg(); }, ":/toolbar/export_img");
    auto actnClose = A_(tr("Exit"), this, &PlotWindow::close);
    auto menuFile = M_(tr("File"), {actnNew, 0, actnExport, _actionOpenImg, actnClose});
    new Ori::Widgets::MruMenuPart(_mru, menuFile, actnClose, this);
    menuBar()->addMenu(menuFile);

    _actionBeamInfo = A_(tr("Plot Beam Info"), this, [this]{ _plot->setBeamInfoVisible(_actionBeamInfo->isChecked(), true); });
    _actionBeamInfo->setCheckable(true);

    auto colorGroup = new QActionGroup(this);
    _actionGrayscale = A_(tr("Grayscale"), colorGroup, [this]{ _plot->setRainbowEnabled(false, true); });
    _actionRainbow = A_(tr("Rainbow"), colorGroup, [this]{ _plot->setRainbowEnabled(true, true); });
    _actionGrayscale->setCheckable(true);
    _actionRainbow->setCheckable(true);
    _actionZoomFull = A_(tr("Zoom to Sensor"), this, [this]{ _plot->zoomFull(true); }, ":/toolbar/zoom_sensor", QKeySequence("Ctrl+0"));
    _actionZoomRoi = A_(tr("Zoom to ROI"), this, [this]{ _plot->zoomRoi(true); }, ":/toolbar/zoom_roi", QKeySequence("Ctrl+1"));
    menuBar()->addMenu(M_(tr("View"), {
        _actionBeamInfo, 0,
        _actionGrayscale, _actionRainbow, 0,
        _actionZoomFull, _actionZoomRoi,
    }));

    _actionMeasure = A_(tr("Start Measurements"), this, &PlotWindow::toggleMeasure, ":/toolbar/start", Qt::Key_F9);
    _actionEditRoi = A_(tr("Edit ROI"), this, [this]{ _plot->startEditRoi(); }, ":/toolbar/roi");
    _actionUseRoi = A_(tr("Use ROI"), this, &PlotWindow::toggleRoi, ":/toolbar/roi_rect");
    _actionUseRoi->setCheckable(true);
    _actionCamConfig = A_(tr("Settings..."), this, [this]{ PlotWindow::editCamConfig(-1); }, ":/toolbar/settings");
    menuBar()->addMenu(M_(tr("Camera"), {
        _actionMeasure, 0,
        _actionEditRoi, _actionUseRoi, 0,
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
    _buttonSelectCam = new QToolButton;
    _buttonSelectCam->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    _buttonSelectCam->setPopupMode(QToolButton::InstantPopup);
    _buttonSelectCam->setIcon(QIcon(":/toolbar/cam_select"));
    _buttonSelectCam->setMenu(_camSelectMenu);
    _buttonSelectCam->setStyleSheet("font-weight: bold");
    _buttonSelectCam->setToolTip(tr("Active camera"));

    auto tb = new Ori::Widgets::FlatToolBar;
    addToolBar(tb);
    tb->setMovable(false);
    tb->addWidget(_buttonSelectCam);
    tb->addAction(_actionCamConfig);
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
    connect(_plot, &Plot::roiEdited, this, &PlotWindow::roiEdited);
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

    // resizeEvent is not called when window gets maximized or restored
    if (e->type() == QEvent::WindowStateChange)
        _plot->adjustWidgetSize();
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
    bool isDemo = dynamic_cast<VirtualDemoCamera*>(_camera.get());
    _buttonOpenImg->setVisible(isImage);
    _actionMeasure->setVisible(isDemo);
    _buttonMeasure->setVisible(isDemo);
    _buttonSelectCam->setText("  " + (isImage ? _actionCamImage->text() : _camera->name()));

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
    _plot->setRoi(c.roi);
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
    bool started = (bool)_saver;
    _mru->setDisabled(started);
    _buttonSelectCam->setDisabled(started);
    _actionCamConfig->setDisabled(started);
    _actionEditRoi->setDisabled(started);
    _actionUseRoi->setDisabled(started);
    _actionOpenImg->setDisabled(started);
    _actionMeasure->setText(started ? tr("Stop Measurements") : tr("Start Measurements"));
    _actionMeasure->setIcon(QIcon(started ? ":/toolbar/stop" : ":/toolbar/start"));
}

void PlotWindow::toggleMeasure()
{
    auto cam = dynamic_cast<VirtualDemoCamera*>(_camera.get());
    if (!cam) return;

    if (_saver)
    {
        _saver.reset(nullptr);
        updateActions();
        return;
    }

    auto cfg = MeasureSaver::configure();
    if (!cfg)
        return;
    auto saver = new MeasureSaver();
    auto res = saver->start(*cfg, cam);
    if (!res.isEmpty()) {
        Ori::Dlg::error(tr("Failed to start measuments:\n%1").arg(res));
        return;
    }
    connect(saver, &MeasureSaver::finished, this, [this]{
        toggleMeasure();
        Ori::Dlg::info(tr("<b>Measurements finished</b>"));
    });
    connect(saver, &MeasureSaver::interrupted, this, [this](const QString &error){
        toggleMeasure();
        Ori::Dlg::error(tr("<b>Measurements interrupted</b><p>") + QString(error).replace("\n", "<br>"));
    });
    _saver.reset(saver);
    cam->startMeasure(_saver.get());
    updateActions();
}

void PlotWindow::stopCapture()
{
    if (!_camera) return;
    auto thread = dynamic_cast<QThread*>(_camera.get());
    if (!thread) {
        qWarning() << "Current camera is not thread based, nothing to stop";
        return;
    }
    thread->requestInterruption();
    if (!thread->wait(5000)) {
        qCritical() << "Can not stop camera thread in timeout";
    }
}

void PlotWindow::captureStopped()
{
    showFps(0);
}

void PlotWindow::dataReady()
{
    _statusBar->setVisible(STATUS_NO_DATA, _tableIntf->resultInvalid());
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
    _plot->stopEditRoi(false);
    _plotIntf->cleanResult();
    _tableIntf->cleanResult();
    _itemRenderTime->setText(tr(" Load time "));
    _mru->append(cam->fileName());
    _camera->startCapture();
    // do showCamConfig() after capture(), when image is already loaded and its size gets known
    showCamConfig(false);
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

    _plot->stopEditRoi(false);
    _plotIntf->cleanResult();
    _tableIntf->cleanResult();
    _itemRenderTime->setText(tr(" Render time "));
    auto cam = new VirtualDemoCamera(_plotIntf, _tableIntf, this);
    connect(cam, &VirtualDemoCamera::ready, this, &PlotWindow::dataReady);
    connect(cam, &VirtualDemoCamera::stats, this, &PlotWindow::showFps);
    connect(cam, &VirtualDemoCamera::finished, this, &PlotWindow::captureStopped);
    _camera.reset((Camera*)cam);
    showCamConfig(false);
    _plot->zoomAuto(false);
    _camera->startCapture();
}
