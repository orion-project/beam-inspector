#ifndef PLOT_WINDOW_H
#define PLOT_WINDOW_H

#include <QMainWindow>

class QLabel;

class Plot;

class PlotWindow : public QMainWindow
{
    Q_OBJECT

public:
    PlotWindow(QWidget *parent = nullptr);
    ~PlotWindow();

private:
    Plot *_plot;
    QLabel *_labelCamera, *_labelResolution, *_labelFps;

    void createMenuBar();
    void createStatusBar();
    void createDockPanel();
    void createPlot();
};
#endif // PLOT_WINDOW_H
