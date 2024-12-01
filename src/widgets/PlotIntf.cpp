#include "PlotIntf.h"

#include "plot/BeamGraph.h"

PlotIntf::PlotIntf(QCustomPlot *plot, QCPColorMap *colorMap, QCPColorScale *colorScale, BeamInfoText *beamInfo)
    : _plot(plot), _colorMap(colorMap), _colorScale(colorScale), _beamInfo(beamInfo)
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
    _results.clear(); 
    memset(_beamData->rawData(), 0, sizeof(double)*_w*_h);
    _min = 0;
    _max = 0;
}

void PlotIntf::setResult(const QList<CgnBeamResult>& r, double min, double max)
{
    _results = r;
    _min = min;
    _max = max;
}

template <class T> void trimList(QList<T>& list, int n)
{
    while (list.size() > n)
        delete list.takeLast();
    list.resize(n);
}

void PlotIntf::showResult()
{
    const int resultCount = _results.size();
    for (int i = 0; i < resultCount; i++) {
        const CgnBeamResult& r = _results.at(i);

        const double xc = _scale.pixelToUnit(r.xc);
        const double yc = _scale.pixelToUnit(r.yc);
        const double dx = _scale.pixelToUnit(r.dx);
        const double dy = _scale.pixelToUnit(r.dy);

        if (i >= _beamShapes.size()) {
            _beamShapes.append(new BeamEllipse(_plot));
        }
        auto beam = _beamShapes.at(i);
        beam->xc = xc;
        beam->yc = yc;
        beam->dx = dx;
        beam->dy = dy;
        beam->phi = r.phi;

        if (i >= _beamAxes.size()) {
            _beamAxes.append(new BeamAxes(_plot));
        }
        auto axes = _beamAxes.at(i);
        axes->xc = xc;
        axes->yc = yc;
        axes->dx = dx;
        axes->dy = dy;
        axes->phi = r.phi;
        axes->infinite = resultCount == 1;
    }
    if (_beamShapes.size() > resultCount) {
        trimList(_beamShapes, resultCount);
        trimList(_beamAxes, resultCount);
    }

    if (_beamInfo->visible() && resultCount == 1 && !_results.at(0).nan)
    {
        const CgnBeamResult& r = _results.at(0);
        double eps = qMin(r.dx, r.dy) / qMax(r.dx, r.dy);
        _beamInfo->setText(QStringLiteral("Xc = %1\nYc = %2\nDx = %3\nDy = %4\nφ = %5°\nε = %6")
            .arg(_scale.format(r.xc),
                 _scale.format(r.yc),
                 _scale.format(r.dx),
                 _scale.format(r.dy))
            .arg(r.phi, 0, 'f', 1)
            .arg(eps, 0, 'f', 3));
    }
    else _beamInfo->setText({});

    _colorScale->setDataRange(QCPRange(_min, _max));
    if (_w > 0) _beamData->setKeyRange(QCPRange(0, _scale.pixelToUnit(_w)));
    if (_h > 0) _beamData->setValueRange(QCPRange(0, _scale.pixelToUnit(_h)));
}

template <class T> void toggleVisiblity(QList<T>& list, bool on)
{
    for (auto &item : list)
        item->setVisible(on);
}

void PlotIntf::setRawView(bool on)
{
    toggleVisiblity(_beamShapes, !on);
    toggleVisiblity(_beamAxes, !on);
}
