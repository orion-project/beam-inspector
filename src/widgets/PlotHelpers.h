#ifndef PLOT_HELPERS_H
#define PLOT_HELPERS_H

#include <QColor>

class QCPAxis;
class QCPAxisRect;
class QCPGraph;
class QCPLegend;
class QCustomPlot;

namespace PlotHelpers
{

enum Theme { SYSTEM, LIGHT };

bool isDarkTheme();
bool isDarkTheme(Theme theme);

QColor themeAxisColor(Theme theme);
QColor themeAxisLabelColor(Theme theme);

void setDefaultAxisFormat(QCPAxis *axis, Theme theme);
void setDefaultAxesFormat(QCPAxisRect *axisRect, Theme theme);
void setDefaultLegendFormat(QCPLegend *legend, Theme theme);
void setThemeColors(QCustomPlot *plot, Theme theme);

void copyImage(QCustomPlot *plot, std::function<void(Theme)> setThemeColors);
void copyGraph(QCPGraph *graph);

} // namespace PlotHelpers

#endif // PLOT_HELPERS_H
