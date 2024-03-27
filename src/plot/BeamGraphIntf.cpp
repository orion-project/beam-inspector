#include "BeamGraphIntf.h"

#include "plot/BeamGraph.h"

BeamGraphIntf::BeamGraphIntf(QCPColorMap *colorMap, QCPColorScale *colorScale,
    BeamEllipse *shape, QCPItemStraightLine *lineX, QCPItemStraightLine *lineY)
    : _colorMap(colorMap), _colorScale(colorScale), _shape(shape), _lineX(lineX), _lineY(lineY)
{
}

void BeamGraphIntf::init(int w, int h, int p)
{
    _beamData = new BeamColorMapData(w, h);
    _colorMap->setData(_beamData);
    _colorScale->setDataRange(QCPRange(0, p));
}

double* BeamGraphIntf::rawData() const
{
    return _beamData->rawData();
}

void BeamGraphIntf::invalidate() const
{
    _beamData->invalidate();
}

void BeamGraphIntf::setResult(const CgnBeamResult& r)
{
    auto phi = qDegreesToRadians(r.phi);
    auto cos_phi = cos(phi);
    auto sin_phi = sin(phi);

    _lineX->point1->setCoords(r.xc, r.yc);
    _lineX->point2->setCoords(r.xc + r.dx*cos_phi, r.yc + r.dx*sin_phi);

    _lineY->point1->setCoords(r.xc, r.yc);
    _lineY->point2->setCoords(r.xc + r.dy*sin_phi, r.yc - r.dy*cos_phi);

    _shape->xc = r.xc;
    _shape->yc = r.yc;
    _shape->dx = r.dx;
    _shape->dy = r.dy;
    _shape->phi = r.phi;
}
