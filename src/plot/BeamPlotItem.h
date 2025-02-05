#ifndef BEAM_PLOT_ITEM_H
#define BEAM_PLOT_ITEM_H

#include "qcustomplot/src/item.h"

#include "cameras/CameraTypes.h"

/**
 * Base class for plot items that are set in relative coordinates (0..1).
 * When drawing, their cooridates must be
 *   - rescaled from relative values to sensor units (see @a PixelScale)
 *   - rescaled from sensor units to painter pixels
 */
class BeamPlotItem : public QCPAbstractItem
{
public:
    explicit BeamPlotItem(QCustomPlot *parentPlot);

    void setImageSize(int sensorW, int sensorH, const PixelScale &scale);

    double selectTest(const QPointF&, bool, QVariant*) const override { return 0; }

protected:
    PixelScale _scale;
    double _maxUnitX = 0, _maxUnitY = 0;
    double _maxPixelX = 0, _maxPixelY = 0;

    virtual void updateCoords() = 0;
};

#endif // BEAM_PLOT_ITEM_H
