#ifndef PLOT_EXPORT_H
#define PLOT_EXPORT_H

class QCustomPlot;

#include <QString>

QString exportImageDlg(QCustomPlot* plot, std::function<void()> prepare, std::function<void()> unprepare);

#endif // PLOT_EXPORT_H
