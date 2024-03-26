#ifndef BEAM_COLOR_MAP_H
#define BEAM_COLOR_MAP_H

#include "qcustomplot.h"

/**
 * A thin wrapper around QCPColorMapData providing access to protected fields
 */
class BeamColorMapData : public QCPColorMapData
{
public:
    BeamColorMapData(int w, int h);

    inline double* rawData() { return mData; }
    inline void invalidate() { mDataModified = true; }
};

#endif // BEAM_COLOR_MAP_H
