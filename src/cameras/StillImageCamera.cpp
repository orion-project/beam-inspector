#include "StillImageCamera.h"

#include "cameras/TableIntf.h"
#include "plot/BeamGraphIntf.h"

#include "beam_calc.h"

#include <QApplication>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>

std::optional<StillImageCamera::ImageInfo> StillImageCamera::start(QSharedPointer<BeamGraphIntf> beam, QSharedPointer<TableIntf> table)
{
    QString fileName = QFileDialog::getOpenFileName(qApp->activeWindow(),
                                qApp->tr("Open Beam Image"), "",
                                qApp->tr("Images (*.png *.pgm *.jpg);;All Files (*.*)"));
    if (fileName.isEmpty())
        return {};

    QImage img(fileName);
    if (img.isNull()) {
        QMessageBox::critical(qApp->activeWindow(), qApp->tr("Open Beam Image"), qApp->tr("Unable to to load image file"));
        return {};
    }

    qDebug() << fileName << img.bitPlaneCount() << img.isGrayscale() << img.allGray() << img.format();

    // declare explicitly as const to avoid deep copy
    const uchar* buf = img.bits();

    CgnBeamCalc c;
    c.w = img.width();
    c.h = img.height();
    c.buf = buf;

    CgnBeamResult r;
    cgn_calc_beam_naive(&c, &r);
    beam->init(c.w, c.h, 255);
    beam->setResult(r);
    table->setResult(r, 0, 0);

    return ImageInfo {
        .fileName = QFileInfo(fileName).fileName(),
        .filePath = fileName,
        .width = img.width(),
        .height = img.height(),
        .bits = img.depth(),
    };
}
