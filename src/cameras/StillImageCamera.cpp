#include "StillImageCamera.h"

#include "plot/BeamGraphIntf.h"
#include "widgets/TableIntf.h"

#include "beam_calc.h"

#include "tools/OriSettings.h"
#include "helpers/OriDialogs.h"

#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QMessageBox>

std::optional<CameraInfo> StillImageCamera::start(QSharedPointer<BeamGraphIntf> beam, QSharedPointer<TableIntf> table)
{
    Ori::Settings s;
    s.beginGroup("StillImageCamera");

    QString fileName = QFileDialog::getOpenFileName(qApp->activeWindow(),
                                qApp->tr("Open Beam Image"),
                                s.strValue("recentDir"),
                                qApp->tr("Images (*.png *.pgm *.jpg);;All Files (*.*)"));
    if (fileName.isEmpty())
        return {};

    s.setValue("recentDir", QFileInfo(fileName).dir().absolutePath());

    return start(fileName, beam, table);
}

std::optional<CameraInfo> StillImageCamera::start(const QString& fileName, QSharedPointer<BeamGraphIntf> beam, QSharedPointer<TableIntf> table)
{
    auto settings = CameraSettings();
    settings.print();

    QElapsedTimer timer;

    timer.start();
    QImage img(fileName);
    if (img.isNull()) {
        Ori::Dlg::error(qApp->tr("Unable to to load image file"));
        return {};
    }
    auto loadTime = timer.elapsed();

    auto fmt = img.format();
    if (fmt != QImage::Format_Grayscale8 && fmt != QImage::Format_Grayscale16) {
        Ori::Dlg::error(qApp->tr("Wrong image format, only grayscale images are supported"));
        return {};
    }

    beam->init(img.width(), img.height());

    // declare explicitly as const to avoid deep copy
    const uchar* buf = img.bits();

    CgnBeamCalc c;
    c.w = img.width();
    c.h = img.height();
    c.bits = fmt == QImage::Format_Grayscale8 ? 8 : 16;
    c.buf = (uint8_t*)buf;

    CgnBeamResult r;
    r.x1 = 0, r.x2 = c.w;
    r.y1 = 0, r.y2 = c.h;

    CgnBeamBkgnd b;
    b.iters = 0;
    b.max_iter = 25;
    b.precision = 0.05;
    b.corner_fraction = 0.035;
    b.nT = 3;
    b.mask_diam = 3;
    b.min = 0;
    b.max = 0;
    QVector<double> subtracted;
    if (settings.subtractBackground) {
        subtracted = QVector<double>(c.w*c.h);
        b.subtracted = subtracted.data();
    }

    timer.restart();
    if (settings.subtractBackground) {
        cgn_calc_beam_bkgnd(&c, &b, &r);
    } else {
        cgn_calc_beam_naive(&c, &r);
    }
    auto calcTime = timer.elapsed();

    timer.restart();
    if (settings.subtractBackground) {
        memcpy(beam->rawData(), b.subtracted, sizeof(double)*c.w*c.h);
    } else {
        cgn_copy_to_f64(&c, beam->rawData(), &b.max);
    }
    auto copyTime = timer.elapsed();

    qDebug() << "loadTime:" << loadTime << "calcTime:" << calcTime << "copyTime:" << copyTime << "iters:" << b.iters;

    beam->setResult(r, b.min, b.max);
    table->setResult(r, loadTime, calcTime);

    QFileInfo fi(fileName);
    return CameraInfo {
        .name = fi.fileName(),
        .descr = fi.absoluteFilePath(),
        .width = img.width(),
        .height = img.height(),
        .bits = img.depth(),
    };
}
