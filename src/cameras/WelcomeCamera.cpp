#include "WelcomeCamera.h"

#include "widgets/PlotIntf.h"
#include "widgets/TableIntf.h"

#include "beam_render.h"

#include <QVector>
#include <QElapsedTimer>

WelcomeCamera::WelcomeCamera(PlotIntf *plot, TableIntf *table) : Camera(plot, table, "WelcomeCamera")
{
}

void WelcomeCamera::capture()
{
    QElapsedTimer timer;
    timer.start();

    CgnBeamRender b;
    b.w = width();
    b.h = height();
    b.xc = width()/2;
    b.yc = height()/2;
    b.dx = width()/4;
    b.dy = height()/4;
    b.phi = 0;
    b.p = 255;
    QVector<uint8_t> buf(b.w*b.h);
    b.buf = buf.data();
    cgn_render_beam(&b);

    auto renderTime = timer.elapsed();
    timer.restart();

    CgnBeamResult r;
    memset(&r, 0, sizeof(CgnBeamResult));
    r.xc = b.xc;
    r.yc = b.yc;
    r.dx = b.dx;
    r.dy = b.dy;
    r.phi = b.phi;

    auto calcTime = timer.elapsed();

    _table->setResult(r, renderTime, calcTime);
    _plot->initGraph(b.w, b.h);
    _plot->setResult(r, 0, b.p);
    cgn_render_beam_to_doubles(&b, _plot->rawGraph());
    _plot->invalidateGraph();
}
