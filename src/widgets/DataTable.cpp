#include "DataTable.h"

#include "app/AppSettings.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QMenu>

DataTable::DataTable() : QTableWidget()
{
}

QSize DataTable::sizeHint() const
{
    return {200, 100};
}

void DataTable::contextMenuEvent(QContextMenuEvent* event)
{
    QTableView::contextMenuEvent(event);

    if (!_contextMenu)
    {
        _contextMenu = new QMenu(this);
        _contextMenu->addAction(tr("Copy"), this, &DataTable::copy);
        _contextMenu->addAction(tr("Select All"), this, &DataTable::selectAll);
    }
    _contextMenu->popup(mapToGlobal(event->pos()));
}

void DataTable::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Copy))
        copy();
    else
        QTableView::keyPressEvent(event);
}

void DataTable::copy()
{
    auto selection = selectionModel()->selection();
    if (selection.isEmpty()) return;
    auto range = selection.first();
    int row1 = qMax(1, range.top());
    int row2 = range.bottom();
    int col1 = range.left();
    int col2 = range.right();
    QString res;
    QTextStream s(&res);
    QChar sep = AppSettings::instance().copyResultsSeparator;
    if (sep == ' ' && AppSettings::instance().copyResultsJustified) {
        QMap<int, int> widthByCol;
        for (int r = row1; r <= row2; r++) {
            if (columnSpan(r, 0) > 1)
                continue;
            for (int c = col1; c <= col2; c++) {
                QString text = item(r, c)->text().trimmed();
                widthByCol[c] = qMax(widthByCol.value(c), text.length());
            }
        }
        for (int r = row1; r <= row2; r++) {
            if (columnSpan(r, 0) > 1)
                continue;
            for (int c = col1; c <= col2; c++) {
                QString text = item(r, c)->text().trimmed();
                if (c == 0)
                    s << text.leftJustified(widthByCol[c], ' ');
                else s << text.rightJustified(widthByCol[c], ' ');
                if (c < col2) s << ' ';
            }
            s << '\n';
        }
    } else {
        for (int r = row1; r <= row2; r++) {
            if (columnSpan(r, 0) > 1)
                continue;
            for (int c = col1; c <= col2; c++) {
                s << item(r, c)->text().trimmed();
                if (c < col2) s << sep;
            }
            s << '\n';
        }
    }
    qApp->clipboard()->setText(res);
}
