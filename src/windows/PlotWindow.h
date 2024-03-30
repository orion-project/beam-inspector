#ifndef PLOT_WINDOW_H
#define PLOT_WINDOW_H

#include <QMainWindow>

class QAction;
class QLabel;
class QTableWidget;

class CameraStats;
class Plot;
class TableIntf;
class VirtualDemoCamera;

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
    QLabel *_labelCamera, *_labelResolution, *_labelFps;
    QTableWidget *_table;
    QSharedPointer<TableIntf> _tableIntf;
    QString imageFile;

    void createMenuBar();
    void createToolBar();
    void createStatusBar();
    void createDockPanel();
    void createPlot();

    void startCapture();
    void stopCapture();
    void newWindow();
    void openImage();

    void captureStopped();
    void dataReady();
    void statsReceived(int fps);

    void updateActions(bool started);
    void updateThemeColors();
    void setThemeColors();
};

#endif // PLOT_WINDOW_H
