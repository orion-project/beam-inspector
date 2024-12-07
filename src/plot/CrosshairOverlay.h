#ifndef CROSSHAIRS_OVERLAY_H
#define CROSSHAIRS_OVERLAY_H

#include "BeamPlotItem.h"

struct Crosshair
{
    int pixelX, pixelY;
    double unitX, unitY;
    double x, y;
    QString label;
    QColor color;
    QStaticText text;
    double roiSize;
    double deltaMin, deltaMax;

    void setLabel(const QString &s);
};

class CrosshairsOverlay : public BeamPlotItem
{
public:
    explicit CrosshairsOverlay(QCustomPlot *parentPlot);

    void setEditing(bool on) { _editing = on; }
    bool isEditing() const { return _editing; }

    QList<RoiRect> rois() const;
    QList<QPointF> positions() const;
    QList<GoodnessLimits> goodnessLimits(int expectedSize) const;

    void clear();
    bool isEmpty() const { return _items.isEmpty(); }

    void showContextMenu(const QPoint& pos);

    void save(QSettings *s);
    void load(QSettings *s);
    QString save(const QString& fileName);
    QString load(const QString& fileName);

    std::function<void()> onEdited;

protected:
    void draw(QCPPainter *painter) override;

    void updateCoords() override;

private:
    bool _hasCoords = false;
    QList<Crosshair> _items;
    double _dragX, _dragY;
    bool _editing = false;
    int _draggingIdx = -1;
    int _selectedIdx = -1;
    QPoint _selectedPos;
    QMenu *_menuForEmpty = nullptr;
    QMenu *_menuForItem = nullptr;

    void setItemCoords(Crosshair &c, double plotX, double plotY);
    void mouseMove(QMouseEvent*);
    void mousePress(QMouseEvent*);
    void mouseRelease(QMouseEvent*);
    int itemIndexAtPos(const QPoint& pos);
    void changeItemColor();
    void changeItemLabel();
    void removeItem();
    void addItem();
};

#endif // CROSSHAIRS_OVERLAY_H
