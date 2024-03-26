#include "BeamGraph.h"

BeamColorMapData::BeamColorMapData(int w, int h)
    : QCPColorMapData(w, h, QCPRange(0, w), QCPRange(0, h))
{
}
