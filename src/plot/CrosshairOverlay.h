#ifndef CROSSHAIRS_OVERLAY_H
#define CROSSHAIRS_OVERLAY_H

#include "qcp/src/item.h"

#include "cameras/CameraTypes.h"

struct Crosshair
{
    double pixelX, pixelY;
    double unitX, unitY;
    QString label;
    QColor color;
    QStaticText text;

    void setLabel(const QString &s);
};

class CrosshairsOverlay : public QCPAbstractItem
{
public:
    explicit CrosshairsOverlay(QCustomPlot *parentPlot);

    bool isEditing() const { return _editing; }

    void setImageSize(int sensorW, int sensorH, const PixelScale &scale);

    double selectTest(const QPointF&, bool, QVariant*) const override { return 0; }

    void showContextMenu(const QPoint& pos);

    void save(QSettings *s);
    void load(QSettings *s);

protected:
    void draw(QCPPainter *painter) override;

private:
    QList<Crosshair> _items;
    double _dragX, _dragY;
    bool _editing = true;
    int _draggingIdx = -1;
    int _selectedIdx = -1;
    QPoint _selectedPos;
    double _maxX = 0, _maxY = 0;
    PixelScale _scale;
    QMenu *_menuForEmpty = nullptr;
    QMenu *_menuForItem = nullptr;

    void updateCoords();
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
