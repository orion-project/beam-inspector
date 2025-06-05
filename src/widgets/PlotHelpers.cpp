#include "PlotHelpers.h"

#include "widgets/OriPopupMessage.h"

#include <QApplication>
#include <QPalette>
#include <QStyleHints>

#include "qcustomplot/src/core.h"
#include "qcustomplot/src/painter.h"
#include "qcustomplot/src/layoutelements/layoutelement-axisrect.h"
#include "qcustomplot/src/layoutelements/layoutelement-legend.h"
#include "qcustomplot/src/plottables/plottable-graph.h"

namespace PlotHelpers
{

bool isDarkTheme()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    return false;
#endif
}

bool isDarkTheme(Theme theme)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return theme == Theme::SYSTEM && qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    return false;
#endif
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

void copyImage(QCustomPlot *plot, std::function<void(Theme)> setThemeColors)
{
    QImage image(plot->width(), plot->height(), QImage::Format_RGB32);
    QCPPainter painter(&image);
    setThemeColors(PlotHelpers::LIGHT);
    plot->toPainter(&painter);
    setThemeColors(PlotHelpers::SYSTEM);
    qApp->clipboard()->setImage(image);

    Ori::Gui::PopupMessage::affirm(qApp->tr("Image has been copied to Clipboard"), Qt::AlignRight|Qt::AlignBottom);
}

void copyGraph(QCPGraph *graph)
{
    QString res;
    QTextStream s(&res);
    foreach (const auto& p, graph->data()->rawData()) {
        s << QString::number(p.key) << ',' << QString::number(p.value) << '\n';
    }
    qApp->clipboard()->setText(res);

    Ori::Gui::PopupMessage::affirm(qApp->tr("Data points been copied to Clipboard"), Qt::AlignRight|Qt::AlignBottom);
}

} // namespace PlotHelpers
