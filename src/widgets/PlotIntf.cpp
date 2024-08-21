#include "PlotIntf.h"

#include "plot/BeamGraph.h"

#include "qcp/src/items/item-straightline.h"

PlotIntf::PlotIntf(QCPColorMap *colorMap, QCPColorScale *colorScale, BeamEllipse *beamShape,
    BeamInfoText *beamInfo, QCPItemStraightLine *lineX, QCPItemStraightLine *lineY)
    : _colorMap(colorMap), _colorScale(colorScale), _beamShape(beamShape), _beamInfo(beamInfo), _lineX(lineX), _lineY(lineY)
{
}

void PlotIntf::initGraph(int w, int h)
{
    _w = w;
    _h = h;
    auto d = _colorMap->data();
    if (d->keySize() != w or d->valueSize() != h) {
        _beamData = new BeamColorMapData(w, h);
        _colorMap->setData(_beamData);
    } else {
        _beamData = static_cast<BeamColorMapData*>(d);
    }
}

double* PlotIntf::rawGraph() const
{
    return _beamData->rawData();
}

void PlotIntf::invalidateGraph() const
{
    _beamData->invalidate();
}

void PlotIntf::cleanResult()
{
    memset(&_res, 0, sizeof(CgnBeamResult));
    memset(_beamData->rawData(), 0, sizeof(double)*_w*_h);
    _min = 0;
    _max = 0;
}

void PlotIntf::setResult(const CgnBeamResult& r, double min, double max)
{
    _res = r;
    _min = min;
    _max = max;
}

void PlotIntf::showResult()
{
    const double phi = qDegreesToRadians(_res.phi);
    const double cos_phi = cos(phi);
    const double sin_phi = sin(phi);
    const double xc = _scale.pixelToUnit(_res.xc);
    const double yc = _scale.pixelToUnit(_res.yc);
    const double dx = _scale.pixelToUnit(_res.dx);
    const double dy = _scale.pixelToUnit(_res.dy);

    _lineX->point1->setCoords(xc, yc);
    _lineX->point2->setCoords(xc + dx*cos_phi, yc + dx*sin_phi);

    _lineY->point1->setCoords(xc, yc);
    _lineY->point2->setCoords(xc + dy*sin_phi, yc - dy*cos_phi);

    _beamShape->xc = xc;
    _beamShape->yc = yc;
    _beamShape->dx = dx;
    _beamShape->dy = dy;
    _beamShape->phi = _res.phi;

    if (_beamInfo->visible() && !_res.nan)
    {
        double eps = qMin(_res.dx, _res.dy) / qMax(_res.dx, _res.dy);
        _beamInfo->setText(QStringLiteral("Xc = %1\nYc = %2\nDx = %3\nDy = %4\nφ = %5°\nε = %6")
            .arg(_scale.format(_res.xc),
                 _scale.format(_res.yc),
                 _scale.format(_res.dx),
                 _scale.format(_res.dy))
            .arg(_res.phi, 0, 'f', 1)
            .arg(eps, 0, 'f', 3));
    }
    else _beamInfo->setText({});

    _colorScale->setDataRange(QCPRange(_min, _max));
    if (_w > 0) _beamData->setKeyRange(QCPRange(0, _scale.pixelToUnit(_w)));
    if (_h > 0) _beamData->setValueRange(QCPRange(0, _scale.pixelToUnit(_h)));
}
