#include "BeamPlotItem.h"

BeamPlotItem::BeamPlotItem(QCustomPlot *parentPlot) : QCPAbstractItem(parentPlot)
{
}

void BeamPlotItem::setImageSize(int sensorW, int sensorH, const PixelScale &scale)
{
    _scale = scale;
    _maxPixelX = sensorW;
    _maxPixelY = sensorH;
    _maxUnitX = scale.pixelToUnit(sensorW);
    _maxUnitY = scale.pixelToUnit(sensorH);
    updateCoords();
}
