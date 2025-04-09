#include "PlotHelpers.h"

#include <QApplication>
#include <QPalette>
#include <QStyleHints>

#include "qcustomplot/src/core.h"
#include "qcustomplot/src/layoutelements/layoutelement-axisrect.h"

namespace PlotHelpers
{

QColor themeAxisColor(Theme theme)
{
    bool dark = theme == Theme::SYSTEM && qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    return qApp->palette().color(dark ? QPalette::Light : QPalette::Shadow);
}

void setDefaultAxisFormat(QCPAxis *axis, Theme theme)
{
    axis->setTickLengthIn(0);
    axis->setTickLengthOut(6);
    axis->setSubTickLengthIn(0);
    axis->setSubTickLengthOut(3);
    axis->grid()->setPen(QPen(QColor(100, 100, 100), 0, Qt::DotLine));
    axis->setTickLabelColor(theme == Theme::LIGHT ? Qt::black : qApp->palette().color(QPalette::WindowText));
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

void setThemeColors(QCustomPlot *plot, Theme theme)
{
    plot->setBackground(theme == LIGHT ? QBrush(Qt::white) : qApp->palette().brush(QPalette::Base));
    setDefaultAxesFormat(plot->axisRect(), theme);
}

} // namespace PlotHelpers
