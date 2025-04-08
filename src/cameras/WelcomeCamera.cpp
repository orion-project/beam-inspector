#include "WelcomeCamera.h"

#include "cameras/HardConfigPanel.h"
#include "widgets/PlotIntf.h"
#include "widgets/TableIntf.h"

#include "beam_render.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QVector>
#include <QElapsedTimer>

class WelcomeHardConfigPanel : public HardConfigPanel
{
public:
    WelcomeHardConfigPanel(QWidget *parent) : HardConfigPanel(parent)
    {
        //qDebug() << "WelcomeHardConfigPanel created";

        auto label = new QLabel("WELCOME", this);
        label->setAlignment(Qt::AlignHCenter);
        label->setWordWrap(true);

        auto layout = new QVBoxLayout(this);
        layout->addStretch();
        layout->addWidget(label);
        layout->addStretch();
    }

    ~WelcomeHardConfigPanel()
    {
        //qDebug() << "WelcomeHardConfigPanel deleted";
    }
};

WelcomeCamera::WelcomeCamera(PlotIntf *plot, TableIntf *table) : Camera(plot, table, "WelcomeCamera")
{
    loadConfig();
}

void WelcomeCamera::startCapture()
{
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

    CgnBeamResult r;
    memset(&r, 0, sizeof(CgnBeamResult));
    r.xc = b.xc;
    r.yc = b.yc;
    r.dx = b.dx;
    r.dy = b.dy;
    r.phi = b.phi;

    _table->setResult({r}, {}, {});
    _plot->initGraph(b.w, b.h);
    _plot->setResult({r}, 0, b.p);
    cgn_render_beam_to_doubles(&b, _plot->rawGraph());
    _plot->invalidateGraph();
}

HardConfigPanel* WelcomeCamera::hardConfgPanel(QWidget *parent)
{
    if (!_hardConfigPanel)
        _hardConfigPanel = new WelcomeHardConfigPanel(parent);
    return _hardConfigPanel;
}
