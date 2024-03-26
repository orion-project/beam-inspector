#include "BeamGraphIntf.h"

#include "plot/BeamGraph.h"

BeamGraphIntf::BeamGraphIntf(QCPColorMap *graph) : _graph(graph)
{

}

void BeamGraphIntf::init(int w, int h)
{
    _graphData = new BeamColorMapData(w, h);
    _graph->setData(_graphData);
}

double* BeamGraphIntf::rawData() const
{
    return _graphData->rawData();
}

void BeamGraphIntf::invalidate() const
{
    _graphData->invalidate();
}
