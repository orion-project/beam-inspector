#ifndef BEAM_GRAPH_H
#define BEAM_GRAPH_H

#include "qcp/src/item.h"
#include "qcp/src/plottables/plottable-colormap.h"

#include "cameras/CameraTypes.h"

/**
 * A thin wrapper around QCPColorMapData providing access to protected fields
 */
class BeamColorMapData : public QCPColorMapData
{
public:
    BeamColorMapData(int w, int h);

    inline double* rawData() { return mData; }
    inline void invalidate() { mDataModified = true; }
};

class BeamColorScale : public QCPColorScale
{
public:
    explicit BeamColorScale(QCustomPlot *parentPlot);

    void setFrameColor(const QColor& c);
};

class BeamEllipse : public QCPAbstractItem
{
public:
    explicit BeamEllipse(QCustomPlot *parentPlot);

    QPen pen;
    double xc, yc, dx, dy, phi;

    double selectTest(const QPointF &pos, bool onlySelectable, QVariant *details=nullptr) const override;

protected:
    void draw(QCPPainter *painter) override;
};

class ApertureRect : public QCPAbstractItem
{
public:
    explicit ApertureRect(QCustomPlot *parentPlot);

    void startEdit();
    void stopEdit(bool apply);
    bool isEditing() const { return _editing; }

    SoftAperture aperture() const { return _aperture; }
    void setAperture(const SoftAperture &aperture, bool replot);

    double selectTest(const QPointF &pos, bool onlySelectable, QVariant *details=nullptr) const override;

    std::function<void()> onEdited;
    double maxX = 0;
    double maxY = 0;

protected:
    void draw(QCPPainter *painter) override;

private:
    QPen _pen, _editPen;
    SoftAperture _aperture;
    bool _editing = false;
    bool _dragging = false;
    double _x1, _y1, _x2, _y2, _dragX, _dragY;
    bool _drag0, _dragX1, _dragX2, _dragY1, _dragY2;
    Qt::CursorShape _dragCursor = Qt::ArrowCursor;

    void mouseMove(QMouseEvent*);
    void mousePress(QMouseEvent*);
    void mouseRelease(QMouseEvent*);
    void mouseDoubleClick(QMouseEvent*);
    void showDragCursor(Qt::CursorShape c);
    void resetDragCusrsor() { showDragCursor(Qt::ArrowCursor); }
    void showCoordTooltip(const QPoint &p);
};

#endif // BEAM_GRAPH_H
