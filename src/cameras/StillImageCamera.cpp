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

#define SETTINGS_KEY "StillImageCamera"

bool StillImageCamera::editSettings()
{
    return CameraSettings::editDlg(SETTINGS_KEY);
}

std::optional<CameraInfo> StillImageCamera::start(QSharedPointer<BeamGraphIntf> beam, QSharedPointer<TableIntf> table)
{
    Ori::Settings s;
    s.beginGroup(SETTINGS_KEY);

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
    settings.load(SETTINGS_KEY);
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

    CgnBeamBkgnd g;
    g.iters = 0;
    g.max_iter = settings.maxIters;
    g.precision = settings.precision;
    g.corner_fraction = settings.cornerFraction;
    g.nT = settings.nT;
    g.mask_diam = settings.maskDiam;
    g.min = 0;
    g.max = 0;
    QVector<double> subtracted;
    if (settings.subtractBackground) {
        subtracted = QVector<double>(c.w*c.h);
        g.subtracted = subtracted.data();
    }

    timer.restart();
    if (settings.subtractBackground) {
        cgn_calc_beam_bkgnd(&c, &g, &r);
    } else {
        cgn_calc_beam_naive(&c, &r);
    }
    auto calcTime = timer.elapsed();

    timer.restart();
    if (settings.subtractBackground) {
        memcpy(beam->rawData(), g.subtracted, sizeof(double)*c.w*c.h);
    } else {
        cgn_copy_to_f64(&c, beam->rawData(), &g.max);
    }
    auto copyTime = timer.elapsed();

    qDebug() << "loadTime:" << loadTime << "calcTime:" << calcTime << "copyTime:" << copyTime << "iters:" << g.iters;

    beam->setResult(r, g.min, g.max);
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
