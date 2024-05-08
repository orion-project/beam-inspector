#include "FileSelector.h"

#include "helpers/OriLayouts.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

using namespace Ori::Layouts;

FileSelector::FileSelector(const QString &title) : QGroupBox(title)
{
    if (title.isEmpty())
        setTitle(tr("Target File"));

    _fileName = new QLineEdit;
    connect(_fileName, &QLineEdit::editingFinished, this, &FileSelector::updateStatus);

    _statusLabel = new QLabel;
    _statusIcon = new QLabel;

    _button = new QPushButton(tr("Select..."));
    connect(_button, &QPushButton::clicked, this, &FileSelector::selectFile);

    LayoutV({
        _fileName,
        LayoutH({
            _statusIcon,
            _statusLabel,
            SpaceH(2),
            Stretch(),
            _button
        }),
    }).useFor(this);
}

QString FileSelector::fileName() const
{
    return _fileName->text().trimmed();
}

void FileSelector::setFileName(const QString &fileName)
{
    _fileName->setText(fileName);
    updateStatus();
}

void FileSelector::setFilters(const QList<QPair<QString, QString>> &filters)
{
    _filters.clear();
    _exts.clear();
    for (const auto &filter : filters)
    {
        _filters << filter.first;
        _exts << filter.second;
    }
}

void FileSelector::updateStatus()
{
    QString fn = fileName();
    if (fn.isEmpty()) {
        _statusLabel->setText("<font color=red><b>" + tr("File not selected") + "</b></font>");
        _statusIcon->setPixmap(QIcon(":/toolbar/error").pixmap(16));
        return;
    }
    QFileInfo fi(fn);
    _fileName->setText(fi.absoluteFilePath());
    if (fi.exists()) {
        _statusLabel->setText("<font color=orange><b>" + tr("File exists, will be overwritten") + "</b></font>");
        _statusIcon->setPixmap(QIcon(":/toolbar/exclame").pixmap(16));
        _selectedDir = fi.dir().absolutePath();
    } else {
        auto dir = fi.dir();
        if (dir.exists()) {
            _selectedDir = dir.absolutePath();
            _statusLabel->setText("<font color=green><b>" + tr("File not found, will be created") + "</b></font>");
            _statusIcon->setPixmap(QIcon(":/toolbar/ok").pixmap(16));
        }
        else {
            _statusLabel->setText("<font color=red><b>" + tr("Target directory does not exist") + "</b></font>");
            _statusIcon->setPixmap(QIcon(":/toolbar/error").pixmap(16));
        }
    }
}

bool FileSelector::selectFile()
{
    QFileDialog dlg(qApp->activeModalWidget(), tr("Select Target File"), _selectedDir);
    dlg.setNameFilters(_filters);
    QString selectedFile = fileName();
    if (!selectedFile.isEmpty()) {
        dlg.selectFile(selectedFile);
        QString ext = QFileInfo(selectedFile).suffix().toLower();
        for (const auto& f : _filters) {
            if (f.contains(ext)) {
                dlg.selectNameFilter(f);
                break;
            }
        }
    }
    if (!dlg.exec())
        return false;
    selectedFile = dlg.selectedFiles().at(0);
    if (QFileInfo(selectedFile).suffix().isEmpty()) {
        QString selectedFilter = dlg.selectedNameFilter();
        for (int i = 0; i < _filters.size(); i++)
            if (selectedFilter == _filters.at(i)) {
                selectedFile += "." + _exts.at(i);
                break;
            }
    }
    _fileName->setText(selectedFile);
    updateStatus();
    return true;
}

QString FileSelector::verify()
{
    auto fn = fileName();
    if (fn.isEmpty())
        return tr("Target file not selected");
    if (!QFileInfo(fn).dir().exists())
        return tr("Target directory does not exist");
    return QString();
}
