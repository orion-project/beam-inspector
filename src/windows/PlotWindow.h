#ifndef PLOT_WINDOW_H
#define PLOT_WINDOW_H

#include <QMainWindow>

class QAction;
class QTableWidget;
class QTableWidgetItem;
class QToolButton;

class Camera;
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

protected:
    void closeEvent(class QCloseEvent*) override;
    void changeEvent(QEvent* e) override;

private:
    Plot *_plot;
    QSharedPointer<Camera> _camera;
    QAction *_actionStart, *_actionStop, *_actionOpen, *_actionCamConfig,
        *_actionGrayscale, *_actionRainbow, *_actionBeamInfo,
        *_actionEditRoi, *_actionUseRoi, *_actionZoomFull, *_actionZoomRoi,
        *_actionCamWelcome, *_actionCamImage, *_actionCamDemo;
    QAction *_buttonStart, *_buttonStop, *_buttonOpen;
    QTableWidget *_table;
    QTableWidgetItem *_itemRenderTime;
    TableIntf *_tableIntf;
    PlotIntf *_plotIntf;
    Ori::MruFileList *_mru;
    Ori::Widgets::StatusBar *_statusBar;
    QMenu *_camSelectMenu;
    QToolButton *_buttonSelectCam;
    QString _prevImage;

    void createMenuBar();
    void createToolBar();
    void createStatusBar();
    void createDockPanel();
    void createPlot();
    void restoreState();
    void storeState();

    void newWindow();
    void openImageDlg();
    void startCapture();
    void stopCapture();
    void editCamConfig(int pageId = -1);
    void activateCamWelcome();
    void activateCamImage();
    void activateCamDemo();

    void captureStopped();
    void dataReady();
    void roiEdited();
    void configChanged();

    void openImage(const QString& fileName);
    void updateActions(bool started);
    void updateThemeColors();
    void setThemeColors();
    void showFps(int fps);
    void showCamConfig(bool replot);
    void processImage();
    void toggleRoi();
};

#endif // PLOT_WINDOW_H
