#ifndef BEAM_GRAPH_INTF_H
#define BEAM_GRAPH_INTF_H

class BeamColorMapData;
class QCPColorMap;

/**
 * Provides access to graph data for camera threads
 * without knowledge about specific graph implementation
 * and avoiding necessity to include whole qcustomplot.h into camera modules
 */
class BeamGraphIntf
{
public:
    BeamGraphIntf(QCPColorMap *graph);

    void init(int w, int h);
    double* rawData() const;
    void invalidate() const;

private:
    QCPColorMap *_graph;
    BeamColorMapData *_graphData;
};

#endif // BEAM_GRAPH_INTF_H
