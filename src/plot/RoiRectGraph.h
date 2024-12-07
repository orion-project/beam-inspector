#ifndef ROI_RECT_GRAPH_H
#define ROI_RECT_GRAPH_H

#include "BeamPlotItem.h"

class QSpinBox;

class RoiRectGraph : public BeamPlotItem
{
public:
    explicit RoiRectGraph(QCustomPlot *parentPlot);

    void startEdit();
    void stopEdit(bool apply);
    bool isEditing() const { return _editing; }

    RoiRect roi() const { return _roi; }
    void setRoi(const RoiRect &roi);

    bool isVisible() const { return _isVisible; }
    void setIsVisible(bool on);

    std::function<void()> onEdited;

    double getX1() const { return _x1; }
    double getX2() const { return _x2; }
    double getY1() const { return _y1; }
    double getY2() const { return _y2; }

    void adjustEditorPosition();

protected:
    void draw(QCPPainter *painter) override;

    void updateCoords() override;

private:
    QPen _pen, _editPen;
    RoiRect _roi;
    bool _editing = false;
    bool _dragging = false;
    double _x1, _y1, _x2, _y2, _dragX, _dragY;
    bool _drag0, _dragX1, _dragX2, _dragY1, _dragY2;
    Qt::CursorShape _dragCursor = Qt::ArrowCursor;
    QSpinBox *_seX1, *_seY1, *_seX2, *_seY2, *_seW, *_seH;
    QFrame *_editor = nullptr;
    bool _isVisible = true;

    int roiW() const { return qAbs((int)_x2 - (int)_x1); }
    int roiH() const { return qAbs((int)_y2 - (int)_y1); }

    void mouseMove(QMouseEvent*);
    void mousePress(QMouseEvent*);
    void mouseRelease(QMouseEvent*);
    void mouseDoubleClick(QMouseEvent*);
    void showDragCursor(Qt::CursorShape c);
    void resetDragCusrsor() { showDragCursor(Qt::ArrowCursor); }
    void showCoordTooltip(const QPoint &p);
    void makeEditor();
    void updateVisibility();
};

class RoiRectsGraph : public BeamPlotItem
{
public:
    explicit RoiRectsGraph(QCustomPlot *parentPlot);

    void setRois(const QList<RoiRect> &rois);

protected:
    void draw(QCPPainter *painter) override;

    void updateCoords() override;

private:
    QPen _pen;
    QList<RoiRect> _rois;
    QList<RoiRect> _unitRois;
};

#endif // ROI_RECT_GRAPH_H
