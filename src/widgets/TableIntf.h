#ifndef TABLE_INTF_H
#define TABLE_INTF_H

#include "beam_calc.h"

#include "cameras/CameraTypes.h"

class QTableWidget;

class QTableWidgetItem;

class TableIntf
{
public:
    TableIntf(QTableWidget *table);

    void setRows(const QList<QPair<int, QString>>& rows);
    void setScale(const PixelScale& scale) { _scale = scale; }
    void setResult(const CgnBeamResult& r, const QMap<int, CamTableData>& data);
    void cleanResult();
    void showResult();
    bool resultInvalid() const;

private:
    QTableWidget *_table;
    QTableWidgetItem *_itXc, *_itYc, *_itDx, *_itDy, *_itPhi, *_itEps;
    int _maxStdRow = 0;
    QMap<int, int> _camRows;
    QMap<int, CamTableData> _camData;
    PixelScale _scale;
    CgnBeamResult _res;
    int _warnColor;

    QTableWidgetItem* makeRow(int &row, const QString& title);
};

#endif // TABLE_INTF_H
