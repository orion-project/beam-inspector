#include "StillImageCamera.h"

#include "cameras/TableIntf.h"
#include "plot/BeamGraphIntf.h"

#include "beam_calc.h"

#include "tools/OriSettings.h"
#include "helpers/OriDialogs.h"

#include <QApplication>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>

std::optional<StillImageCamera::ImageInfo> StillImageCamera::start(QSharedPointer<BeamGraphIntf> beam, QSharedPointer<TableIntf> table)
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

std::optional<StillImageCamera::ImageInfo> StillImageCamera::start(const QString& fileName, QSharedPointer<BeamGraphIntf> beam, QSharedPointer<TableIntf> table)
{
    QImage img(fileName);
    if (img.isNull()) {
        Ori::Dlg::error(qApp->tr("Unable to to load image file"));
        return {};
    }

    auto fmt = img.format();
    if (fmt != QImage::Format_Grayscale8 && fmt != QImage::Format_Grayscale16) {
        Ori::Dlg::error(qApp->tr("Unsupported image format, only grayscale images are supported"));
        return {};
    }

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
