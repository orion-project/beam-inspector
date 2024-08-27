#ifndef CROSSHAIRS_OVERLAY_H
#define CROSSHAIRS_OVERLAY_H

#include "qcp/src/item.h"

#include "cameras/CameraTypes.h"

struct Crosshair
{
    int pixelX, pixelY;
    double unitX, unitY;
    double x, y;
    QString label;
    QColor color;
    QStaticText text;

    void setLabel(const QString &s);
};

class CrosshairsOverlay : public QCPAbstractItem
{
public:
    explicit CrosshairsOverlay(QCustomPlot *parentPlot);

    void setEditing(bool on) { _editing = on; }
    bool isEditing() const { return _editing; }

    void clear();
    bool isEmpty() const { return _items.isEmpty(); }

    void setImageSize(int sensorW, int sensorH, const PixelScale &scale);

    double selectTest(const QPointF&, bool, QVariant*) const override { return 0; }

    void showContextMenu(const QPoint& pos);

    void save(QSettings *s);
    void load(QSettings *s);
    QString save(const QString& fileName);
    QString load(const QString& fileName);

protected:
    void draw(QCPPainter *painter) override;

private:
    bool _hasCoords = false;
    QList<Crosshair> _items;
    double _dragX, _dragY;
    bool _editing = false;
    int _draggingIdx = -1;
    int _selectedIdx = -1;
    QPoint _selectedPos;
    double _maxUnitX = 0, _maxUnitY = 0;
    double _maxPixelX = 0, _maxPixelY = 0;
    PixelScale _scale;
    QMenu *_menuForEmpty = nullptr;
    QMenu *_menuForItem = nullptr;

    void updateCoords();
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
