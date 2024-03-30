#include "StillImageCamera.h"

#include "plot/BeamGraphIntf.h"
#include "widgets/TableIntf.h"

#include "beam_calc.h"
#include "beam_render.h"

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

    beam->init(img.width(), img.height(), 1);

    // declare explicitly as const to avoid deep copy
    const uchar* buf = img.bits();

    timer.restart();
    qint64 calcTime;

    CgnBeamResult r;
    if (fmt == QImage::Format_Grayscale8) {
        CgnBeamCalc8 c;
        c.w = img.width();
        c.h = img.height();
        c.buf = (const uint8_t*)buf;
        cgn_calc_beam_8_naive(&c, &r);
        calcTime = timer.elapsed();

        timer.restart();
        cgn_render_beam_to_doubles_norm_8(c.buf, c.w*c.h, beam->rawData());
    } else {
        CgnBeamCalc16 c;
        c.w = img.width();
        c.h = img.height();
        c.buf = (const uint16_t*)buf;
        cgn_calc_beam_16_naive(&c, &r);
        calcTime = timer.elapsed();

        timer.restart();
        cgn_render_beam_to_doubles_norm_16(c.buf, c.w*c.h, beam->rawData());
    }

    auto copyTime = timer.elapsed();

    qDebug() << "loadTime" << loadTime << "calcTime" << calcTime << "copyTime" << copyTime;

    beam->setResult(r);
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
