#ifndef PLOT_EXPORT_H
#define PLOT_EXPORT_H

#include <cameras/CameraTypes.h>

#include <QByteArray>
#include <QImage>

class QCustomPlot;

struct ExportImgArg
{
    QByteArray data;
    int width;
    int height;
    int bpp;
    std::optional<RawFrameData> raw;
};

void exportImageDlg(QCustomPlot* plot, std::function<void()> prepare, std::function<void()> unprepare);
void exportImageDlg(const ExportImgArg &arg);
void exportImageDlg(QImage img);

#endif // PLOT_EXPORT_H
