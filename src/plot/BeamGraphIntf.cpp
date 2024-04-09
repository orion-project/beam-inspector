#include "BeamGraphIntf.h"

#include "plot/BeamGraph.h"

#include "qcustomplot/items/item-straightline.h"
#include "qcustomplot/items/item-text.h"

BeamGraphIntf::BeamGraphIntf(QCPColorMap *colorMap, QCPColorScale *colorScale, BeamEllipse *beamShape,
    QCPItemText *beamInfo, QCPItemStraightLine *lineX, QCPItemStraightLine *lineY)
    : _colorMap(colorMap), _colorScale(colorScale), _beamShape(beamShape), _beamInfo(beamInfo), _lineX(lineX), _lineY(lineY)
{
}

void BeamGraphIntf::init(int w, int h)
{
    _beamData = new BeamColorMapData(w, h);
    _colorMap->setData(_beamData);
}

double* BeamGraphIntf::rawData() const
{
    return _beamData->rawData();
}

void BeamGraphIntf::invalidate() const
{
    _beamData->invalidate();
}

void BeamGraphIntf::setResult(const CgnBeamResult& r, double min, double max)
{
    auto phi = qDegreesToRadians(r.phi);
    auto cos_phi = cos(phi);
    auto sin_phi = sin(phi);

    _lineX->point1->setCoords(r.xc, r.yc);
    _lineX->point2->setCoords(r.xc + r.dx*cos_phi, r.yc + r.dx*sin_phi);

    _lineY->point1->setCoords(r.xc, r.yc);
    _lineY->point2->setCoords(r.xc + r.dy*sin_phi, r.yc - r.dy*cos_phi);

    _beamShape->xc = r.xc;
    _beamShape->yc = r.yc;
    _beamShape->dx = r.dx;
    _beamShape->dy = r.dy;
    _beamShape->phi = r.phi;

    _beamInfo->setText(QStringLiteral("Xc=%1\nYc=%2\nDx=%3\nDy=%4\nφ=%5°")
        .arg(int(r.xc)).arg(int(r.yc)).arg(int(r.dx)).arg(int(r.dy)).arg(r.phi, 0, 'f', 1));

    _colorScale->setDataRange(QCPRange(min, max));
}
