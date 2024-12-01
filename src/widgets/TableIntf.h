#ifndef TABLE_INTF_H
#define TABLE_INTF_H

#include "beam_calc.h"

#include "cameras/CameraTypes.h"

class QTableWidget;

class QTableWidgetItem;

using ResultId = int;
using RowIndex = int;

class TableIntf
{
public:
    TableIntf(QTableWidget *table);

    void setRows(const TableRowsSpec &rows);
    void setScale(const PixelScale& scale) { _scale = scale; }
    void setResult(const QList<CgnBeamResult>& r, const QMap<ResultId, CamTableData>& data);
    void cleanResult();
    void showResult();
    bool resultInvalid() const;
    bool isPowerRow(QTableWidgetItem *item);

private:
    struct ResultRows
    {
        QTableWidgetItem *xc, *yc, *dx, *dy, *phi, *eps;
    };
    QTableWidget *_table;
    ResultId _powerResult = -1;
    QMap<ResultId, RowIndex> _camRows;
    QMap<ResultId, CamTableData> _camData;
    QList<ResultRows> _resRows;
    QList<CgnBeamResult> _resData;
    PixelScale _scale;
    int _warnColor;

    QTableWidgetItem* makeHeader(RowIndex &row, const QString& title);
    QTableWidgetItem* makeRow(RowIndex &row, const QString& title);
};

#endif // TABLE_INTF_H
