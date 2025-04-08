#ifndef PLOT_HELPERS_H
#define PLOT_HELPERS_H

#include <QColor>

class QCPAxis;
class QCPAxisRect;
class QCustomPlot;

namespace PlotHelpers
{

enum Theme { SYSTEM, LIGHT };

QColor themeAxisColor(Theme theme);
void setDefaultAxisFormat(QCPAxis *axis, Theme theme);
void setDefaultAxesFormat(QCPAxisRect *axisRect, Theme theme);
void setThemeColors(QCustomPlot *plot, Theme theme);

} // namespace PlotHelpers

#endif // PLOT_HELPERS_H
