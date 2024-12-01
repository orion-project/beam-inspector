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

    if (qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark)
        _warnColor = 0xFFFF8C00;
    else _warnColor = 0xffffdcbc;
}

void TableIntf::setRows(const TableRowsSpec &rows)
{
    if (rows.results.isEmpty())
        return;
    _table->setRowCount(0);
    _resRows.clear();
    _camRows.clear();
    int row = 0;
    for (const auto &title : rows.results) {
        makeHeader(row, title);
        ResultRows it;
        it.xc = makeRow(row, qApp->tr("Center X"));
        it.yc = makeRow(row, qApp->tr("Center Y"));
        it.dx = makeRow(row, qApp->tr("Width X"));
        it.dy = makeRow(row, qApp->tr("Width Y"));
        it.phi = makeRow(row, qApp->tr("Azimuth"));
        it.eps = makeRow(row, qApp->tr("Ellipticity"));
        _resRows << it;
    }
    if (!rows.aux.isEmpty()) {
        makeHeader(row, qApp->tr("Camera"));
        for (auto it = rows.aux.constBegin(); it != rows.aux.constEnd(); it++) {
            _camRows.insert(it->first, row);
            makeRow(row, it->second);
        }
    }
}

QTableWidgetItem* TableIntf::makeHeader(RowIndex &row, const QString& title)
{
    _table->setRowCount(row+1);
    auto it = new QTableWidgetItem(title);
    auto f = it->font();
    f.setBold(true);
    it->setFont(f);
    it->setTextAlignment(Qt::AlignCenter);
    it->setBackground(_table->palette().brush(QPalette::Button));
    _table->setItem(row, 0, it);
    _table->setSpan(row, 0, 1, 2);
    row++;
    return it;
}

QTableWidgetItem* TableIntf::makeRow(RowIndex &row, const QString& title)
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
    _table->setItem(row, 1, it);
    row++;
    return it;
};

void TableIntf::cleanResult()
{
    _resData.clear();
    _camData.clear();
}

void TableIntf::setResult(const QList<CgnBeamResult>& r, const QMap<ResultId, CamTableData>& data)
{
    _resData = r;
    _camData = data;
}

inline void setTextInvald(QTableWidgetItem *it)
{
    it->setText(QStringLiteral(" --- "));
}

static QString formatPower(double v, int f)
{
    switch (f)
    {
    case -3: return QStringLiteral(" %1 mW ").arg(v, 0, 'f', 3);
    case -6: return QStringLiteral(" %1 uW ").arg(v, 0, 'f', 3);
    case -9: return QStringLiteral(" %1 nW ").arg(v, 0, 'f', 3);
    }
    return QStringLiteral(" %1 W ").arg(v, 0, 'f', 3);
}

void TableIntf::showResult()
{
    for (auto it = _camData.constBegin(); it != _camData.constEnd(); it++) {
        ResultId resultId = it.key();
        RowIndex row = _camRows.value(resultId, -1);
        if (row < 0) return;
        auto item = _table->item(row, 1);
        const auto &data = it.value();
        QString text;
        switch (data.type) {
            case CamTableData::TEXT:
                text = data.value.toString();
                break;
            case CamTableData::MS:
                text = QStringLiteral(" %1 ms ").arg(qRound(data.value.toDouble()));
                break;
            case CamTableData::COUNT:
                text = QStringLiteral(" %1 ").arg(data.value.toInt());
                break;
            case CamTableData::POWER: {
                auto p = data.value.toList();
                text = formatPower(p[0].toDouble(), p[1].toInt());
                if (data.warn)
                    item->setToolTip(qApp->tr("Recalibration required"));
                else item->setToolTip({});
                _powerResult = resultId;
                break;
            }
            case CamTableData::VALUE3:
                text = QStringLiteral(" %1 ").arg(data.value.toDouble(), 0, 'f', 3);
                break;
        }
        if (data.warn)
            text += QStringLiteral(" (!)");
        item->setText(text);
        item->setBackground(data.warn ? QColor(_warnColor) : Qt::transparent);
    }

    for (int i = 0; i < _resData.size(); i++) {
        if (i >= _resRows.size()) {
            break;
        }
        const auto& res = _resData.at(i);
        const auto& row = _resRows.at(i);
        if (res.nan) {
            setTextInvald(row.xc);
            setTextInvald(row.yc);
            setTextInvald(row.dx);
            setTextInvald(row.dy);
            setTextInvald(row.phi);
            setTextInvald(row.eps);
            continue;
        }

        double eps = qMin(res.dx, res.dy) / qMax(res.dx, res.dy);
        row.xc->setText(_scale.formatWithMargins(res.xc));
        row.yc->setText(_scale.formatWithMargins(res.yc));
        row.dx->setText(_scale.formatWithMargins(res.dx));
        row.dy->setText(_scale.formatWithMargins(res.dy));
        row.phi->setText(QStringLiteral(" %1° ").arg(res.phi, 0, 'f', 1));
        row.eps->setText(QStringLiteral(" %1 ").arg(eps, 0, 'f', 3));
    }
    for (int i = _resData.size(); i < _resRows.size(); i++) {
        const auto &row = _resRows.at(i);
        setTextInvald(row.xc);
        setTextInvald(row.yc);
        setTextInvald(row.dx);
        setTextInvald(row.dy);
        setTextInvald(row.phi);
        setTextInvald(row.eps);
    }
}

bool TableIntf::resultInvalid() const
{
    return _resData.isEmpty();
}

bool TableIntf::isPowerRow(QTableWidgetItem *item)
{
    for (auto it = _camRows.cbegin(); it != _camRows.cend(); it++)
        if (it.key() == _powerResult && it.value() == item->row())
            return true;
    return false;
}
