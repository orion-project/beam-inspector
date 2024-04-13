#include "PlotIntf.h"

#include "plot/BeamGraph.h"

#include "qcp/src/items/item-straightline.h"
#include "qcp/src/items/item-text.h"

PlotIntf::PlotIntf(QCPColorMap *colorMap, QCPColorScale *colorScale, BeamEllipse *beamShape,
    QCPItemText *beamInfo, QCPItemStraightLine *lineX, QCPItemStraightLine *lineY)
    : _colorMap(colorMap), _colorScale(colorScale), _beamShape(beamShape), _beamInfo(beamInfo), _lineX(lineX), _lineY(lineY)
{
}

void PlotIntf::initGraph(int w, int h)
{
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
    auto phi = qDegreesToRadians(_res.phi);
    auto cos_phi = cos(phi);
    auto sin_phi = sin(phi);

    _lineX->point1->setCoords(_res.xc, _res.yc);
    _lineX->point2->setCoords(_res.xc + _res.dx*cos_phi, _res.yc + _res.dx*sin_phi);

    _lineY->point1->setCoords(_res.xc, _res.yc);
    _lineY->point2->setCoords(_res.xc + _res.dy*sin_phi, _res.yc - _res.dy*cos_phi);

    _beamShape->xc = _res.xc;
    _beamShape->yc = _res.yc;
    _beamShape->dx = _res.dx;
    _beamShape->dy = _res.dy;
    _beamShape->phi = _res.phi;

    if (_beamInfo->visible()) {
        double eps = qMin(_res.dx, _res.dy) / qMax(_res.dx, _res.dy);
        _beamInfo->setText(QStringLiteral("Xc=%1\nYc=%2\nDx=%3\nDy=%4\nφ=%5°\nε=%6")
            .arg(_scale.format(_res.xc),
                 _scale.format(_res.yc),
                 _scale.format(_res.dx),
                 _scale.format(_res.dy))
            .arg(_res.phi, 0, 'f', 1)
            .arg(eps, 0, 'f', 3));
    }

    _colorScale->setDataRange(QCPRange(_min, _max));
}
