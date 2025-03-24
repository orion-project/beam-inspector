#ifndef DATA_TABLE_H
#define DATA_TABLE_H

#include <QTableWidget>

QT_BEGIN_NAMESPACE
class QMenu;
QT_END_NAMESPACE

class DataTable : public QTableWidget
{
public:
    DataTable();

    QSize sizeHint() const override;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QMenu *_contextMenu = nullptr;

    void copy();
};

#endif // DATA_TABLE_H

