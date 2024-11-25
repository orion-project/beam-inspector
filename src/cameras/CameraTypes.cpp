#include "CameraTypes.h"

#include "core/OriFloatingPoint.h"

#include <QApplication>
#include <QRandomGenerator>
#include <QSettings>

QString formatSecs(int secs) {
    int h = secs / 3600;
    int m = (secs - h * 3600) / 60;
    int s = secs - h * 3600 - m * 60;
    QStringList strs;
    if (h > 0) strs << QStringLiteral("%1h").arg(h);
    if (m > 0) strs << QStringLiteral("%1m").arg(m);
    if (s > 0) strs << QStringLiteral("%1s").arg(s);
    return strs.join(' ');
}

int PowerMeter::minAvgFrames = 1;
int PowerMeter::maxAvgFrames = 10;

//------------------------------------------------------------------------------
//                               CameraConfig
//------------------------------------------------------------------------------

#define LOAD(option, type, def) option = s->value(QStringLiteral(#option), def).to ## type()
#define SAVE(option) s->setValue(QStringLiteral(#option), option)

void CameraConfig::load(QSettings *s)
{
    LOAD(plot.normalize, Bool, true);
    LOAD(plot.rescale, Bool, false);
    LOAD(plot.fullRange, Bool, true);
    LOAD(plot.customScale.on, Bool, true);
    LOAD(plot.customScale.factor, Double, 5);
    LOAD(plot.customScale.unit, String, "um");

    LOAD(bgnd.on, Bool, true);
    LOAD(bgnd.iters, Int, 0);
    LOAD(bgnd.precision, Double, 0.05);
    LOAD(bgnd.corner, Double, 0.035);
    LOAD(bgnd.noise, Double, 3);
    LOAD(bgnd.mask, Double, 3);

    LOAD(roi.on, Bool, false);
    LOAD(roi.left, Double, 0.25);
    LOAD(roi.top, Double, 0.25);
    LOAD(roi.right, Double, 0.75);
    LOAD(roi.bottom, Double, 0.75);
    roiMode = RoiMode(s->value("roiMode", ROI_NONE).toInt());
    rois.clear();
    int roiCount = s->beginReadArray("rois");
    for (int i = 0; i < roiCount; i++) {
        s->setArrayIndex(i);
        RoiRect r;
        r.on = s->value("on", true).toBool();
        r.left = s->value("left", 0.25).toDouble();
        r.top = s->value("top", 0.25).toDouble();
        r.right = s->value("right", 0.75).toDouble();
        r.bottom = s->value("bottom", 0.75).toDouble();
        rois << r;
    }
    s->endArray();

    LOAD(power.on, Bool, false);
    LOAD(power.avgFrames, Int, 4);
    LOAD(power.power, Double, 0.0);
    LOAD(power.decimalFactor, Int, 0);
}

void CameraConfig::save(QSettings *s, bool compact) const
{
    SAVE(plot.normalize);
    SAVE(plot.rescale);
    SAVE(plot.fullRange);
    SAVE(plot.customScale.on);
    if (!compact or plot.customScale.on) {
        SAVE(plot.customScale.factor);
        SAVE(plot.customScale.unit);
    }

    SAVE(bgnd.on);
    if (!compact or bgnd.on) {
        SAVE(bgnd.iters);
        SAVE(bgnd.precision);
        SAVE(bgnd.corner);
        SAVE(bgnd.noise);
        SAVE(bgnd.mask);
    }

    SAVE(roi.on);
    if (!compact or roi.on) {
        SAVE(roi.left);
        SAVE(roi.top);
        SAVE(roi.right);
        SAVE(roi.bottom);
    }
    SAVE(roiMode);
    s->beginWriteArray("rois", rois.size());
    for (int i = 0; i < rois.size(); i++) {
        const auto& roi = rois.at(i);
        s->setArrayIndex(i);
        s->setValue("on", roi.on);
        if (!compact or roi.on) {
            s->setValue("left", roi.left);
            s->setValue("top", roi.top);
            s->setValue("right", roi.right);
            s->setValue("bottom", roi.bottom);
        }
    }
    s->endArray();

    SAVE(power.on);
    if (!compact or power.on) {
        SAVE(power.avgFrames);
        SAVE(power.power);
        SAVE(power.decimalFactor);
    }
}

//------------------------------------------------------------------------------
//                               PixelScale
//------------------------------------------------------------------------------

QString PixelScale::format(const double &v) const
{
    if (on)
        return QStringLiteral("%1 %2").arg(v*factor, 0, 'f', 1).arg(unit);
    return QStringLiteral("%1").arg(v, 0, 'f', 1);
}

QString PixelScale::formatWithMargins(const double &v) const
{
    if (on)
        return QStringLiteral(" %1 %2 ").arg(v*factor, 0, 'f', 1).arg(unit);
    return QStringLiteral(" %1 ").arg(v, 0, 'f', 1);
}

bool PixelScale::operator ==(const PixelScale& s) const {
    return
        on == s.on &&
        int(factor * 1000) == int(s.factor * 1000) &&
        unit == s.unit;
}

//------------------------------------------------------------------------------
//                               RoiRect
//------------------------------------------------------------------------------

bool RoiRect::isValid() const
{
    return left >= 0 && right > left && right <= 1 && top >= 0 && bottom > top && top <= 1;
}

bool RoiRect::isZero() const
{
    return Double(left).isZero() && Double(top).isZero() && Double(right).isZero() && Double(bottom).isZero();
}

QString RoiRect::sizeStr(int w, int h) const
{
    return QStringLiteral("%1 × %2")
        .arg(qRound(double(w)*(right-left)))
        .arg(qRound(double(h)*(bottom-top)));
}

void RoiRect::fix()
{
    if (left > right) std::swap(left, right);
    if (top > bottom) std::swap(top, bottom);
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > 1) right = 1;
    if (bottom > 1) bottom = 1;
    if (Double(left).is(right)) {
        if (Double(right).is(1)) left = 0;
        else right = 1;
    }
    if (Double(top).is(bottom)) {
        if (Double(bottom).is(1)) top = 0;
        else bottom = 1;
    }
}

//------------------------------------------------------------------------------
//                               RandomOffset
//------------------------------------------------------------------------------

double RandomOffset::rnd() const
{
    return QRandomGenerator::global()->generate() / rnd_max;
}

RandomOffset::RandomOffset(double min, double max) : RandomOffset((min + max)/2.0, min, max)
{
}

RandomOffset::RandomOffset(double start, double min, double max) : c(start), v(start), v_min(min), v_max(max)
{
    if (min > max)
        std::swap(min, max);
    dv = v_max - v_min;
    h = dv / 4.0;
    rnd_max = double(std::numeric_limits<quint32>::max());
}

double RandomOffset::next()
{
    v = qAbs(v + rnd()*h - h*0.5);
    if (v > dv)
        v = dv - rnd()*h;
    c = v + v_min;
    return c;
}

//------------------------------------------------------------------------------
//                               CameraCommons
//------------------------------------------------------------------------------

QString CameraCommons::supportedImageFilters()
{
    return qApp->tr("Images (*.png *.pgm *.jpg);;All Files (*.*)");
}
