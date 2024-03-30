#ifndef PLOT_WINDOW_H
#define PLOT_WINDOW_H

#include <QMainWindow>

class QAction;
class QTableWidget;

class CameraStats;
class Plot;
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

private:
    Plot *_plot;
    VirtualDemoCamera *_cameraThread = nullptr;
    QAction *_actionStart, *_actionStop, *_actionOpen;
    QTableWidget *_table;
    QSharedPointer<TableIntf> _tableIntf;
    QString imageFile;
    Ori::MruFileList *_mru;
    Ori::Widgets::StatusBar *_statusBar;

    void createMenuBar();
    void createToolBar();
    void createStatusBar();
    void createDockPanel();
    void createPlot();

    void newWindow();
    void openImageDlg();
    void startCapture();
    void stopCapture();

    void captureStopped();
    void dataReady();

    void openImage(const QString& fileName);
    void updateActions(bool started);
    void updateThemeColors();
    void setThemeColors();
    void showFps(int fps);
};

#endif // PLOT_WINDOW_H
