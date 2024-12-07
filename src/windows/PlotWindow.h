#ifndef PLOT_WINDOW_H
#define PLOT_WINDOW_H

#include <QMainWindow>
#include <QPointer>

class QAction;
class QTableWidget;
class QTableWidgetItem;
class QToolButton;

class Camera;
struct CameraStats;
class HardConfigPanel;
class MeasureProgressBar;
class MeasureSaver;
class Plot;
class PlotIntf;
class TableIntf;

namespace Ori {
class MruFileList;
namespace Widgets {
class StatusBar;
}}

class PlotWindow : public QMainWindow
{
    Q_OBJECT

public:
    PlotWindow(QWidget *parent = nullptr);
    ~PlotWindow();

signals:
    void camConfigChanged();

protected:
    void closeEvent(class QCloseEvent*) override;
    void changeEvent(QEvent* e) override;
    bool event(QEvent *event) override;

private:
    Plot *_plot;
    QSharedPointer<Camera> _camera;
    QSharedPointer<MeasureSaver> _saver;
    QAction *_actionMeasure, *_actionOpenImg, *_actionCamConfig,
        *_actionBeamInfo, *_actionLoadColorMap, *_actionCleanColorMaps,
        *_actionEditRoi, *_actionUseRoi, *_actionZoomFull, *_actionZoomRoi,
        *_actionCamWelcome, *_actionCamImage, *_actionCamDemoRender, *_actionCamDemoImage, *_actionRefreshCams,
        *_actionResultsPanel, *_actionHardConfig, *_actionSaveRaw, *_actionRawView,
        *_actionCrosshairsShow, *_actionCrosshairsEdit, *_actionSetCamCustomName,
        *_actionSetupPowerMeter, *_actionUseMultiRoi;
    QAction *_buttonMeasure, *_buttonOpenImg;
    QActionGroup *_colorMapActions;
    QTableWidget *_table;
    TableIntf *_tableIntf;
    PlotIntf *_plotIntf;
    Ori::MruFileList *_mru;
    Ori::Widgets::StatusBar *_statusBar;
    QMenu *_camSelectMenu, *_colorMapMenu;
    QToolButton *_buttonSelectCam;
    QString _prevImage;
    MeasureProgressBar *_measureProgress;
    QDockWidget *_resultsDock, *_hardConfigDock;
    HardConfigPanel *_stubConfigPanel = nullptr;
    HardConfigPanel *_camConfigPanel = nullptr;
    QMap<QString, QString> _camCustomNames;

    void createDockPanel();
    void createMenuBar();
    void createPlot();
    void createStatusBar();
    void createToolBar();

    void restoreState();
    void storeState();

    // Action handlers
    void activateCamWelcome();
    void activateCamImage();
    void activateCamDemoRender();
    void activateCamDemoImage();
#ifdef WITH_IDS
    void activateCamIds();
#endif
    void devResizeWindow();
    void devResizeDock(QDockWidget*);
    void editCamConfig(int pageId = -1);
    void editRoi();
    void editRoiCfg();
    void editRoisSize();
    void newWindow();
    void openImageDlg();
    void selectColorMapFile();
    void setCamCustomName();
    void setupPowerMeter();
    void toggleCrosshairsEditing();
    void toggleCrosshairsVisbility();
    void toggleHardConfig();
    void toggleMeasure(bool force);
    void toggleRawView();
    void toggleResultsPanel();
    void toggleRoi();
    void toggleMultiRoi();

    void captureStopped();
    void configChanged();
    void dataReady();
    void roiEdited();
    void crosshairsEdited();
    void statsReceived(const CameraStats &stats);
    void resultsTableDoubleClicked(QTableWidgetItem *item);

    void fillCamSelector();
    void openImage(const QString& fileName);
    void processImage();
    void setThemeColors();
    void showFps(double fps, double hardFps);
    void showCamConfig(bool replot);
    void showSelectedCamera();
    void stopCapture();
    void updateControls();
    void updateColorMapMenu();
    void updateHardConfgPanel();
    void updateThemeColors();
};

#endif // PLOT_WINDOW_H
