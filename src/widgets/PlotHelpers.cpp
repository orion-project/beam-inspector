#include "PlotHelpers.h"

#include <QApplication>
#include <QPalette>
#include <QStyleHints>

#include "qcustomplot/src/core.h"
#include "qcustomplot/src/layoutelements/layoutelement-axisrect.h"
#include "qcustomplot/src/layoutelements/layoutelement-legend.h"


namespace PlotHelpers
{

bool isDarkTheme(Theme theme)
{
    return theme == Theme::SYSTEM && qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

QColor themeAxisColor(Theme theme)
{
    return qApp->palette().color(isDarkTheme(theme) ? QPalette::Light : QPalette::Shadow);
}

QColor themeAxisLabelColor(Theme theme)
{
    return theme == Theme::SYSTEM ? qApp->palette().color(QPalette::WindowText) : Qt::black;
}

void setDefaultAxisFormat(QCPAxis *axis, Theme theme)
{
    axis->setTickLengthIn(0);
    axis->setTickLengthOut(6);
    axis->setSubTickLengthIn(0);
    axis->setSubTickLengthOut(3);
    axis->grid()->setPen(QPen(QColor(100, 100, 100), 0, Qt::DotLine));
    axis->setTickLabelColor(themeAxisLabelColor(theme));
    auto pen = QPen(themeAxisColor(theme), 0, Qt::SolidLine);
    axis->setTickPen(pen);
    axis->setSubTickPen(pen);
    axis->setBasePen(pen);
}

void setDefaultAxesFormat(QCPAxisRect *axisRect, Theme theme)
{
    foreach (auto axis, axisRect->axes())
        setDefaultAxisFormat(axis, theme);
}

void setDefaultLegendFormat(QCPLegend *legend, Theme theme)
{
    legend->setBrush(isDarkTheme(theme) ? qApp->palette().color(QPalette::Window) : Qt::white);
    legend->setBorderPen(QPen(themeAxisColor(theme), 0, Qt::SolidLine));
    legend->setTextColor(themeAxisLabelColor(theme));
}

void setThemeColors(QCustomPlot *plot, Theme theme)
{
    plot->setBackground(theme == LIGHT ? QBrush(Qt::white) : qApp->palette().brush(QPalette::Base));
    setDefaultAxesFormat(plot->axisRect(), theme);
    setDefaultLegendFormat(plot->legend, theme);
}

} // namespace PlotHelpers
