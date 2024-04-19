#include "MeasureSaver.h"

#include "cameras/CameraTypes.h"

#include <QDebug>
#include <QThread>
#include <QFile>

MeasureSaver::MeasureSaver() : QObject()
{
    _fileName = "G:\\Projects\\cignffus\\tmp\\result.csv";
}

MeasureSaver::~MeasureSaver()
{
    _thread->quit();
    _thread->wait();
    qDebug() << "MeasureSaver: stopped";
}

QString MeasureSaver::start()
{
    QFile f(_fileName);
    if (!f.open(QIODeviceBase::WriteOnly | QIODeviceBase::Truncate))
        return tr("Failed to create resuls file:\n%1").arg(f.errorString());
    QTextStream out(&f);
    out << "Index,Timestamp,Center X,Center Y,Width X,Width Y,Azimuth,Ellipticity\n";

    _thread.reset(new QThread);
    moveToThread(_thread.get());
    _thread->start();
    qDebug() << "MeasureSaver: started" << QThread::currentThreadId();

    return {};
}

bool MeasureSaver::event(QEvent *event)
{
    auto e = dynamic_cast<MeasureEvent*>(event);
    if (!e) return QObject::event(event);

    qDebug() << "MeasureSaver: measurement" << e->num;

    QFile f(_fileName);
    if (!f.open(QIODeviceBase::WriteOnly | QIODeviceBase::Append)) {
        qCritical() << "Failed to open resuls file" << f.errorString();
        return true;
    }

    QTextStream out(&f);
    for (auto r = e->results; r - e->results < e->count; r++) {
        out << r->idx << ','
            << e->start.addMSecs(r->time).toString(Qt::ISODateWithMs) << ',';
        if (r->nan)
            out << 0 << ',' // xc
                << 0 << ',' // yc
                << 0 << ',' // dx
                << 0 << ',' // dy
                << 0 << ',' // phi
                << 0;       // eps
        else
            out << int(r->xc * _scale) << ','
                << int(r->yc * _scale) << ','
                << int(r->dx * _scale) << ','
                << int(r->dy * _scale) << ','
                << QString::number(r->phi, 'f', 1) << ','
                << QString::number(qMin(r->dx, r->dy) / qMax(r->dx, r->dy), 'f', 3);
        out << '\n';
    }

    if (e->num == 1)
        emit finished();

    return true;
}
