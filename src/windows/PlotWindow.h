#ifndef PLOT_WINDOW_H
#define PLOT_WINDOW_H

#include <QMainWindow>

class QAction;
class QTableWidget;
class QTableWidgetItem;
class QToolButton;

class Camera;
struct CameraStats;
class IdsComfort;
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

private:
    Plot *_plot;
    QSharedPointer<IdsComfort> _ids;
    QSharedPointer<Camera> _camera;
    QSharedPointer<MeasureSaver> _saver;
    QAction *_actionMeasure, *_actionOpenImg, *_actionCamConfig,
        *_actionGrayscale, *_actionRainbow, *_actionBeamInfo,
        *_actionEditRoi, *_actionUseRoi, *_actionZoomFull, *_actionZoomRoi,
        *_actionCamWelcome, *_actionCamImage, *_actionCamDemo, *_actionRefreshCams;
    QAction *_buttonMeasure, *_buttonOpenImg;
    QTableWidget *_table;
    QTableWidgetItem *_itemRenderTime, *_itemErrCount;
    TableIntf *_tableIntf;
    PlotIntf *_plotIntf;
    Ori::MruFileList *_mru;
    Ori::Widgets::StatusBar *_statusBar;
    QMenu *_camSelectMenu;
    QToolButton *_buttonSelectCam;
    QString _prevImage;
    MeasureProgressBar *_measureProgress;

    void createMenuBar();
    void createToolBar();
    void createStatusBar();
    void createDockPanel();
    void createPlot();
    void restoreState();
    void storeState();
    void fillCamSelector();

    void newWindow();
    void openImageDlg();
    void toggleMeasure(bool force);
    void stopCapture();
    void editCamConfig(int pageId = -1);
    void activateCamWelcome();
    void activateCamImage();
    void activateCamDemo();
    void activateCamIds();

    void statsReceived(const CameraStats &stats);
    void captureStopped();
    void dataReady();
    void roiEdited();
    void configChanged();

    void openImage(const QString& fileName);
    void updateActions();
    void updateThemeColors();
    void setThemeColors();
    void showFps(int fps);
    void showCamConfig(bool replot);
    void processImage();
    void toggleRoi();
};

#endif // PLOT_WINDOW_H
