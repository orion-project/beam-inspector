#include "TableIntf.h"

#include <QApplication>
#include <QHeaderView>
#include <QStyleHints>
#include <QTableWidget>

TableIntf::TableIntf(QTableWidget *table) : _table(table)
{
    _table->setColumnCount(2);
    _table->setHorizontalHeaderLabels({ qApp->tr("Name"), qApp->tr("Value") });
    _table->verticalHeader()->setVisible(false);
    _table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _table->setSelectionBehavior(QAbstractItemView::SelectRows);
    auto h = _table->horizontalHeader();
    h->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    h->setSectionResizeMode(1, QHeaderView::Stretch);
    h->setHighlightSections(false);

    int row = _maxStdRow;
    auto makeHeader = [this, &row](const QString& title) {
        _table->setRowCount(row+1);
        auto it = new QTableWidgetItem(title);
        auto f = it->font();
        f.setBold(true);
        it->setFont(f);
        it->setTextAlignment(Qt::AlignCenter);
        _table->setItem(row, 0, it);
        _table->setSpan(row, 0, 1, 2);
        row++;
    };

    makeHeader(qApp->tr(" Centroid "));
    _itXc = makeRow(row, qApp->tr("Center X"));
    _itYc = makeRow(row, qApp->tr("Center Y"));
    _itDx = makeRow(row, qApp->tr("Width X"));
    _itDy = makeRow(row, qApp->tr("Width Y"));
    _itPhi = makeRow(row, qApp->tr("Azimuth"));
    _itEps = makeRow(row, qApp->tr("Ellipticity"));
    makeHeader(qApp->tr(" Camera "));
    _maxStdRow = row;

    if (qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark)
        _warnColor = 0xFFFF8C00;
    else _warnColor = 0xffffdcbc;
}

void TableIntf::setRows(const QList<QPair<int, QString>>& rows)
{
    for (int row = _table->rowCount()-1; row >= _maxStdRow; row--)
        _table->removeRow(row);
    _camRows.clear();
    _camData.clear();
    int row = _maxStdRow;
    for (auto it = rows.constBegin(); it != rows.constEnd(); it++) {
        _camRows.insert(it->first, row);
        makeRow(row, it->second);
    }
}

QTableWidgetItem* TableIntf::makeRow(int &row, const QString& title)
{
    _table->setRowCount(row+1);
    auto it = new QTableWidgetItem(" " + title + " ");
    auto f = it->font();
    f.setBold(true);
    f.setPointSize(f.pointSize()+1);
    it->setFont(f);
    it->setBackground(_table->palette().brush(QPalette::Button));
    _table->setItem(row, 0, it);

    it = new QTableWidgetItem(" --- ");
    f.setBold(false);
    it->setFont(f);
    _table->setItem(row++, 1, it);
    return it;
};

void TableIntf::cleanResult()
{
    memset(&_res, 0, sizeof(CgnBeamResult));
    _camData.clear();
}

void TableIntf::setResult(const CgnBeamResult& r, const QMap<int, CamTableData> &data)
{
    _res = r;
    _camData = data;
}

inline void setTextInvald(QTableWidgetItem *it)
{
    it->setText(QStringLiteral(" --- "));
}

void TableIntf::showResult()
{
    for (auto it = _camData.constBegin(); it != _camData.constEnd(); it++) {
        int row = _camRows.value(it.key(), -1);
        if (row < 0) return;
        auto item = _table->item(row, 1);
        const auto &data = it.value();
        switch (data.type) {
            case CamTableData::NONE:
                item->setText(data.value.toString());
                break;
            case CamTableData::MS:
                item->setText(QStringLiteral(" %1 ms ").arg(qRound(data.value.toDouble())));
                break;
            case CamTableData::COUNT:
                item->setText(QStringLiteral(" %1 ").arg(data.value.toInt()));
                break;
            case CamTableData::VALUE3:
            item->setText(QStringLiteral(" %1 ").arg(data.value.toDouble(), 0, 'f', 3));
                break;
        }
        item->setBackground(data.warn ? QColor(_warnColor) : Qt::transparent);
    }

    if (_res.nan)
    {
        setTextInvald(_itXc);
        setTextInvald(_itYc);
        setTextInvald(_itDx);
        setTextInvald(_itDy);
        setTextInvald(_itPhi);
        setTextInvald(_itEps);
        return;
    }

    double eps = qMin(_res.dx, _res.dy) / qMax(_res.dx, _res.dy);
    _itXc->setText(_scale.formatWithMargins(_res.xc));
    _itYc->setText(_scale.formatWithMargins(_res.yc));
    _itDx->setText(_scale.formatWithMargins(_res.dx));
    _itDy->setText(_scale.formatWithMargins(_res.dy));
    _itPhi->setText(QStringLiteral(" %1° ").arg(_res.phi, 0, 'f', 1));
    _itEps->setText(QStringLiteral(" %1 ").arg(eps, 0, 'f', 3));
}

bool TableIntf::resultInvalid() const
{
    return _res.nan;
}
