#include "CamPropControls.h"

#include <QApplication>
#include <QStyle>

namespace CamPropControls
{

int layoutTopMargin()
{
    return qApp->style()->pixelMetric(QStyle::PM_LayoutTopMargin);
}

int layoutBottomMargin()
{
    return qApp->style()->pixelMetric(QStyle::PM_LayoutBottomMargin);
}

int layoutLeftMargin()
{
    return qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin);
}

int layoutRightMargin()
{
    return qApp->style()->pixelMetric(QStyle::PM_LayoutRightMargin);
}

int layoutSpacing()
{
    return qApp->style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing);
}

int scrollBarWidth()
{
    return qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
}

}
