#include "TableIntf.h"

#include "app/AppSettings.h"
#include "widgets/PlotHelpers.h"

#include <QApplication>
#include <QHeaderView>
#include <QStyleHints>
#include <QTableWidget>

TableIntf::TableIntf(QTableWidget *table) : _table(table)
{
    _table->verticalHeader()->setVisible(false);
    _table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _table->setSelectionBehavior(QAbstractItemView::SelectRows);

    if (PlotHelpers::isDarkTheme()) {
        _warnColor = 0xFFFF8C00;
        _silentColor = 0xFF444477;
    } else {
        _warnColor = 0xffffdcbc;
        _silentColor = 0xFF8888AA;
    }
}

void TableIntf::setRows(const TableRowsSpec &rows)
{
    _showSdev = rows.showSdev;
    if (_showSdev) {
        _table->setColumnCount(3);
        _table->setHorizontalHeaderLabels({ qApp->tr("Name"), qApp->tr("AVG"), qApp->tr("SDEV") });
    } else {
        _table->setColumnCount(2);
        _table->setHorizontalHeaderLabels({ qApp->tr("Name"), qApp->tr("Value") });
    }
    auto h = _table->horizontalHeader();
    h->setSectionResizeMode(QHeaderView::Stretch);
    h->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    h->setHighlightSections(false);

    const auto &s = AppSettings::instance();

    _table->setRowCount(0);
    _resRows.clear();
    _camRows.clear();
    int row = 0;
    for (const auto &title : rows.results) {
        makeHeader(row, title);
        ResultRows it;
        if (s.tableShowDX)
            it.dx = makeRow(row, qApp->tr("Width X"));
        if (s.tableShowDY)
            it.dy = makeRow(row, qApp->tr("Width Y"));
        if (s.tableShowEps)
            it.eps = makeRow(row, qApp->tr("Ellipticity"));
        if (s.tableShowXC)
            it.xc = makeRow(row, qApp->tr("Center X"), true);
        if (s.tableShowYC)
            it.yc = makeRow(row, qApp->tr("Center Y"), true);
        if (s.tableShowPhi)
            it.phi = makeRow(row, qApp->tr("Azimuth"), true);
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
    _table->setSpan(row, 0, 1, _table->columnCount());
    row++;
    return it;
}

TableIntf::ResultRow TableIntf::makeRow(RowIndex &row, const QString& title, bool silent)
{
    _table->setRowCount(row+1);
    auto it = new QTableWidgetItem(" " + title + " ");
    auto f = it->font();
    f.setBold(true);
    f.setPointSize(f.pointSize()+1);
    it->setFont(f);
    it->setBackground(_table->palette().brush(QPalette::Button));
    if (silent)
        it->setForeground(QColor(_silentColor));
    _table->setItem(row, 0, it);

    auto itVal = new QTableWidgetItem(" --- ");
    f.setBold(false);
    itVal->setFont(f);
    if (silent)
        itVal->setForeground(QColor(_silentColor));
    _table->setItem(row, 1, itVal);

    QTableWidgetItem *itSdev = nullptr;
    if (_showSdev) {
        itSdev = new QTableWidgetItem(" --- ");
        itSdev->setFont(f);
        _table->setItem(row, 2, itSdev);
    }

    row++;
    return { itVal, itSdev };
};

void TableIntf::cleanResult()
{
    _resData.clear();
    _resSdev.clear();
    _camData.clear();
}

void TableIntf::setResult(const QList<CgnBeamResult>& val, const QList<CgnBeamResult>& sdev, const QMap<ResultId, CamTableData>& data)
{
    _resData = val;
    _resSdev = sdev;
    _camData = data;
}

void TableIntf::setTextInvald(const std::optional<TableIntf::ResultRow> &row)
{
    if (!row) return;
    row->val->setText(QStringLiteral(" --- "));
    if (row->sdev)
        row->sdev->setText(QStringLiteral(" --- "));
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
        for (int col = 0; col < _table->columnCount(); col++)
            _table->item(row, col)->setToolTip({});
        auto item = _table->item(row, 1);
        const auto &data = it.value();
        QString text, text1, tooltip;
        bool isPower = false;
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
                isPower = true;
                auto p = data.value.toList();
                auto val = p[0].toDouble();
                auto sdev = p[1].toDouble();
                auto scale = p[2].toInt();
                text = formatPower(val, scale);
                if (_showSdev)
                    text1 = formatPower(sdev, scale);
                if (data.warn)
                    tooltip = qApp->tr("Recalibration required");
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
        if (isPower && _showSdev) {
            if (data.warn)
                text1 += QStringLiteral(" (!)");
            auto item = _table->item(row, 2);
            item->setText(text1);
            item->setBackground(data.warn ? QColor(_warnColor) : Qt::transparent);
        }
        if (!tooltip.isEmpty())
            for (int col = 0; col < _table->columnCount(); col++)
                _table->item(row, col)->setToolTip(tooltip);
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
        if (row.xc) row.xc->val->setText(_scale.formatWithMargins(res.xc));
        if (row.yc) row.yc->val->setText(_scale.formatWithMargins(res.yc));
        if (row.dx) row.dx->val->setText(_scale.formatWithMargins(res.dx));
        if (row.dy) row.dy->val->setText(_scale.formatWithMargins(res.dy));
        if (row.phi) row.phi->val->setText(QStringLiteral(" %1° ").arg(res.phi, 0, 'f', 1));
        if (row.eps) row.eps->val->setText(QStringLiteral(" %1 ").arg(eps, 0, 'f', 3));

        if (_showSdev && i <= _resSdev.size() - 1) {
            const auto &res = _resSdev.at(i);
            double eps = qMin(res.dx, res.dy) / qMax(res.dx, res.dy);
            if (row.xc) row.xc->sdev->setText(_scale.formatWithMargins(res.xc));
            if (row.yc) row.yc->sdev->setText(_scale.formatWithMargins(res.yc));
            if (row.dx) row.dx->sdev->setText(_scale.formatWithMargins(res.dx));
            if (row.dy) row.dy->sdev->setText(_scale.formatWithMargins(res.dy));
            if (row.phi) row.phi->sdev->setText(QStringLiteral(" %1° ").arg(res.phi, 0, 'f', 1));
            if (row.eps) row.eps->sdev->setText(QStringLiteral(" %1 ").arg(eps, 0, 'f', 3));
        }
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
