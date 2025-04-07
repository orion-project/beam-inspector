#ifndef BEAM_GRAPH_H
#define BEAM_GRAPH_H

#include "qcustomplot/src/colorgradient.h"
#include "qcustomplot/src/item.h"
#include "qcustomplot/src/items/item-straightline.h"
#include "qcustomplot/src/plottables/plottable-colormap.h"

//------------------------------------------------------------------------------

class BeamColorMap : public QCPColorMap
{
public:
    explicit BeamColorMap(QCPAxis *keyAxis, QCPAxis *valueAxis);

    void setGradient(const QCPColorGradient &gradient);
};

//------------------------------------------------------------------------------

class BeamColorMapData : public QCPColorMapData
{
public:
    BeamColorMapData(int w, int h);

    inline double* rawData() { return mData; }
    inline void invalidate() { mDataModified = true; }
};

//------------------------------------------------------------------------------

class BeamColorScale : public QCPColorScale
{
public:
    explicit BeamColorScale(QCustomPlot *parentPlot);

    void setFrameColor(const QColor& c);
    void setGradient(const QCPColorGradient &gradient);
};

//------------------------------------------------------------------------------

class BeamEllipse : public QCPAbstractItem
{
public:
    explicit BeamEllipse(QCustomPlot *parentPlot);

    QPen pen;
    double xc, yc, dx, dy, phi;

    double selectTest(const QPointF&, bool, QVariant*) const override { return 0; }

protected:
    void draw(QCPPainter *painter) override;
};

//------------------------------------------------------------------------------

class BeamAxes : public QCPItemStraightLine
{
public:
    explicit BeamAxes(QCustomPlot *parentPlot);

    QPen penX, penY;
    double xc, yc, dx, dy, phi;
    bool infinite = false;

    double selectTest(const QPointF&, bool, QVariant*) const override { return 0; }

protected:
    void draw(QCPPainter *painter) override;
};

//------------------------------------------------------------------------------

class BeamInfoText : public QCPAbstractItem
{
public:
    explicit BeamInfoText(QCustomPlot *parentPlot);

    double selectTest(const QPointF&, bool, QVariant*) const override { return 0; }

    void setText(const QString& text) { _text = text; }

protected:
    void draw(QCPPainter *painter) override;

    QString _text;
};

//------------------------------------------------------------------------------

class OverexposureWarning : public QCPAbstractItem
{
public:
    explicit OverexposureWarning(QCustomPlot *parentPlot);

    double selectTest(const QPointF&, bool, QVariant*) const override { return 0; }

protected:
    void draw(QCPPainter *painter) override;

private:
    QTextOption opts;
    QFont font;
    QBrush brush;
    QPen pen;
};

//------------------------------------------------------------------------------

class PrecalculatedGradient : public QCPColorGradient
{
public:
    PrecalculatedGradient(const QString& presetFile);

private:
    bool loadColors(const QString& presetFile);
    void applyDefaultColors();
};

#endif // BEAM_GRAPH_H
