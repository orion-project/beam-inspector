#ifndef PLOT_EXPORT_H
#define PLOT_EXPORT_H

class QCustomPlot;

#include <QByteArray>

void exportImageDlg(QCustomPlot* plot, std::function<void()> prepare, std::function<void()> unprepare);
void exportImageDlg(QByteArray data, int w, int h, bool hdr);

#endif // PLOT_EXPORT_H
