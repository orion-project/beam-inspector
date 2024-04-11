#ifndef PLOT_WINDOW_H
#define PLOT_WINDOW_H

#include <QMainWindow>

class QAction;
class QTableWidget;
class QTableWidgetItem;

class CameraInfo;
class Plot;
class PlotIntf;
class TableIntf;
class VirtualDemoCamera;

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
    VirtualDemoCamera *_cameraThread = nullptr;
    QAction *_actionStart, *_actionStop, *_actionOpen, *_actionCamSettings,
        *_actionGrayscale, *_actionRainbow, *_actionBeamInfo;
    QTableWidget *_table;
    QTableWidgetItem *_itemRenderTime;
    TableIntf *_tableIntf;
    PlotIntf *_plotIntf;
    QString imageFile;
    Ori::MruFileList *_mru;
    Ori::Widgets::StatusBar *_statusBar;
    QString _stillImageFile;

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
    void editCamSettings();

    void captureStopped();
    void dataReady();
    void imageReady(const CameraInfo& info);

    void openImage(const QString& fileName);
    void updateActions(bool started);
    void updateIsoWarning();
    void updateThemeColors();
    void setThemeColors();
    void showFps(int fps);
    void showInfo(const CameraInfo& info);
};

#endif // PLOT_WINDOW_H
