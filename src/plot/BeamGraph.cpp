#include "BeamGraph.h"

#include "qcustomplot/src/core.h"
#include "qcustomplot/src/painter.h"

//------------------------------------------------------------------------------
//                               BeamColorMapData
//------------------------------------------------------------------------------

BeamColorMap::BeamColorMap(QCPAxis *keyAxis, QCPAxis *valueAxis) : QCPColorMap(keyAxis, valueAxis)
{
}

void BeamColorMap::setGradient(const QCPColorGradient &gradient)
{
    // Base setGradient() checks for gradient equality and skips if they are same.
    // PrecalculatedGradient always gets equal to another one even when they colors differ.
    // So redeclare setGradient() to get rid of equality check
    mGradient = gradient;
    mMapImageInvalidated = true;
    emit gradientChanged(mGradient);
}

//------------------------------------------------------------------------------
//                               BeamColorMapData
//------------------------------------------------------------------------------

BeamColorMapData::BeamColorMapData(int w, int h)
    : QCPColorMapData(w, h, QCPRange(0, w), QCPRange(0, h))
{
}

//------------------------------------------------------------------------------
//                               BeamColorScale
//------------------------------------------------------------------------------

class BeamColorScaleAxisRectPrivate : public QCPColorScaleAxisRectPrivate
{
    friend class BeamColorScale;
};

BeamColorScale::BeamColorScale(QCustomPlot *parentPlot) : QCPColorScale(parentPlot)
{
    setRangeDrag(false);
    setRangeZoom(false);
}

void BeamColorScale::setFrameColor(const QColor& c)
{
    foreach (auto a, mAxisRect->axes())
        a->setBasePen(QPen(c, 0, Qt::SolidLine));
}

void BeamColorScale::setGradient(const QCPColorGradient &gradient)
{
    // Base setGradient() checks for gradient equality and skips if they are same.
    // PrecalculatedGradient always gets equal to another one even when they colors differ.
    // So redeclare setGradient() to get rid of equality check
    // This also requries overriding of pimpl-class QCPColorScaleAxisRectPrivate (see above).
    mGradient = gradient;
    if (mAxisRect)
        if (auto axisRect = reinterpret_cast<BeamColorScaleAxisRectPrivate*>(mAxisRect.get()); axisRect)
            axisRect->mGradientImageInvalidated = true;
    emit gradientChanged(mGradient);
}

//------------------------------------------------------------------------------
//                                BeamEllipse
//------------------------------------------------------------------------------

BeamEllipse::BeamEllipse(QCustomPlot *parentPlot) : QCPAbstractItem(parentPlot)
{
    pen.setColor(Qt::white);
}

void BeamEllipse::draw(QCPPainter *painter)
{
    if (!qIsFinite(xc) || !qIsFinite(yc) || !qIsFinite(dx) || !qIsFinite(dy)) return;
    const double x = parentPlot()->xAxis->coordToPixel(xc);
    const double y = parentPlot()->yAxis->coordToPixel(yc);
    const double rx = parentPlot()->xAxis->coordToPixel(xc + dx/2.0) - x;
    const double ry = parentPlot()->yAxis->coordToPixel(yc + dy/2.0) - y;
    auto t = painter->transform();
    painter->translate(x, y);
    painter->rotate(phi);
    painter->setPen(pen);
    painter->drawEllipse(QPointF(), rx, ry);
    painter->setTransform(t);
}

//------------------------------------------------------------------------------
//                                BeamAxes
//------------------------------------------------------------------------------

BeamAxes::BeamAxes(QCustomPlot *parentPlot) : QCPItemStraightLine(parentPlot)
{
    penX.setColor(Qt::yellow);
    penY.setColor(Qt::white);
}

void BeamAxes::draw(QCPPainter *painter)
{
    if (!qIsFinite(xc) || !qIsFinite(yc) || !qIsFinite(dx) || !qIsFinite(dy)) return;
    const double x = parentPlot()->xAxis->coordToPixel(xc);
    const double y = parentPlot()->yAxis->coordToPixel(yc);
    const double dx = parentPlot()->xAxis->coordToPixel(xc + this->dx) - x;
    const double dy = parentPlot()->yAxis->coordToPixel(yc + this->dy) - y;
    if (infinite) {
        const double rad_phi = qDegreesToRadians(phi);
        const double cos_phi = cos(rad_phi);
        const double sin_phi = sin(rad_phi);
        auto lineX = getRectClippedStraightLine({x, y}, {dx*cos_phi, dx*sin_phi}, clipRect());
        if (!lineX.isNull()) {
            painter->setPen(penX);
            painter->drawLine(lineX);
        }
        auto lineY = getRectClippedStraightLine({x, y}, {dy*sin_phi, -dy*cos_phi}, clipRect());
        if (!lineY.isNull()) {
            painter->setPen(penY);
            painter->drawLine(lineY);
        }
    } else {
        auto t = painter->transform();
        painter->translate(x, y);
        painter->rotate(phi);
        painter->setPen(penX);
        painter->drawLine(QPointF(-dx, 0), QPointF(dx, 0));
        painter->setPen(penY);
        painter->drawLine(QPointF(0, -dy), QPointF(0, dy));
        painter->setTransform(t);
    }
}

//------------------------------------------------------------------------------
//                               BeamInfoText
//------------------------------------------------------------------------------

BeamInfoText::BeamInfoText(QCustomPlot *parentPlot) : QCPAbstractItem(parentPlot)
{
}

void BeamInfoText::draw(QCPPainter *painter)
{
    auto r = parentPlot()->axisRect()->rect();
    painter->setPen(Qt::white);
    painter->drawText(QRect(r.left()+15, r.top()+10, 10, 10), Qt::TextDontClip, _text);
}

//------------------------------------------------------------------------------
//                             PredefinedGradient
//------------------------------------------------------------------------------

PrecalculatedGradient::PrecalculatedGradient(const QString& presetFile) : QCPColorGradient()
{
    if (!loadColors(presetFile))
        applyDefaultColors();
}

bool PrecalculatedGradient::loadColors(const QString& presetFile)
{
#define WARN qWarning() << "Loading gradient" << presetFile << ':'
    QFile file(presetFile);
    if (!file.open(QIODevice::ReadOnly)) {
        WARN << file.errorString();
        return false;
    }
    int lineNo = 0;
    QTextStream stream(&file);
    QVector<QRgb> colors;
    while (!stream.atEnd()) {
        lineNo++;
        QString line  = stream.readLine();
        if (line.isEmpty()) continue;
        QStringView view(line);
        auto parts = view.split(',');
        if (parts.size() != 3) {
            WARN << "Line" << lineNo << ": Expected three values";
            continue;
        }
        bool ok;
        int r = parts.at(0).toInt(&ok);
        if (!ok) {
            WARN << "Line" << lineNo << ": R is not int";
            continue;
        }
        int g = parts.at(1).toInt(&ok);
        if (!ok) {
            WARN << "Line" << lineNo << ": G is not int";
            continue;
        }
        int b = parts.at(2).toInt(&ok);
        if (!ok) {
            WARN << "Line" << lineNo << ": B is not int";
            continue;
        }
        colors << qRgb(r, g, b);
    }
    mColorBufferInvalidated = false;
    mLevelCount = colors.size();
    mColorBuffer = colors;
    return true;
#undef WARN
}

void PrecalculatedGradient::applyDefaultColors()
{
//    Grad0 in img/gradient.svg
//    QMap<double, QColor> colors {
//        { 0.0, QColor(0x2b053e) },
//        { 0.1, QColor(0xc2077c) },
//        { 0.15, QColor(0xbe05f3) },
//        { 0.2, QColor(0x2306fb) },
//        { 0.3, QColor(0x0675db) },
//        { 0.4, QColor(0x05f9ee) },
//        { 0.5, QColor(0x04ca04) },
//        { 0.65, QColor(0xfafd05) },
//        { 0.8, QColor(0xfc8705) },
//        { 0.9, QColor(0xfc4d06) },
//        { 1.0, QColor(0xfc5004) },
//    };

//    Grad1 in img/gradient.svg
//    QMap<double, QColor> colors {
//        { 0.00, QColor(0x2b053e) },
//        { 0.05, QColor(0xc4138a) },
//        { 0.10, QColor(0x9e0666) },
//        { 0.15, QColor(0xbe05f3) },
//        { 0.20, QColor(0x2306fb) },
//        { 0.30, QColor(0x0675db) },
//        { 0.40, QColor(0x05f9ee) },
//        { 0.50, QColor(0x04ca04) },
//        { 0.65, QColor(0xfafd05) },
//        { 0.80, QColor(0xfc8705) },
//        { 0.90, QColor(0xfc4d06) },
//        { 1.00, QColor(0xfc5004) },
//    };

    // Grad2 in img/gradient.svg
    QMap<double, QColor> colors {
        { 0.0, QColor(0x2b053e) },
        { 0.075, QColor(0xd60c8a) },
        { 0.15, QColor(0xbe05f3) },
        { 0.2, QColor(0x2306fb) },
        { 0.3, QColor(0x0675db) },
        { 0.4, QColor(0x05f9ee) },
        { 0.5, QColor(0x04ca04) },
        { 0.65, QColor(0xfafd05) },
        { 0.8, QColor(0xfc8705) },
        { 0.9, QColor(0xfc4d06) },
        { 1.0, QColor(0xfc5004) },
    };

    setColorStops(colors);
}
