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
};

class CrosshairsOverlay : public QCPAbstractItem
{
public:
    explicit CrosshairsOverlay(QCustomPlot *parentPlot);

    void setImageSize(int sensorW, int sensorH, const PixelScale &scale);

    double selectTest(const QPointF&, bool, QVariant*) const override { return 0; }

protected:
    void draw(QCPPainter *painter) override;

private:
    QList<Crosshair> _crosshairs;
    double _dragX, _dragY;
    bool _editing = true;
    int _draggingIdx = -1;
    double _maxX = 0, _maxY = 0;
    PixelScale _scale;

    void updateCoords();
    void mouseMove(QMouseEvent*);
    void mousePress(QMouseEvent*);
    void mouseRelease(QMouseEvent*);
};

#endif // CROSSHAIRS_OVERLAY_H
