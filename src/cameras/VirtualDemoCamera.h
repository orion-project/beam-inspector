#ifndef VIRTUAL_DEMO_CAMERA_H
#define VIRTUAL_DEMO_CAMERA_H

#include <QThread>
#include <QSharedPointer>

class BeamGraphIntf;
class BeamRenderer;
class TableIntf;

class VirtualDemoCamera : public QThread
{
    Q_OBJECT

public:
    explicit VirtualDemoCamera(QSharedPointer<BeamGraphIntf> beam, QSharedPointer<TableIntf> table, QObject *parent = nullptr);

signals:
    void ready();
    void stats(int fps);

protected:
    void run() override;

private:
    QSharedPointer<BeamRenderer> _render;
};

#endif // VIRTUAL_DEMO_CAMERA_H
