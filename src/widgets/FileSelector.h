#ifndef FILE_SELECTOR_H
#define FILE_SELECTOR_H

#include <QGroupBox>

class QLabel;
class QLineEdit;
class QPushButton;

class FileSelector : public QGroupBox
{
    Q_OBJECT

public:
    FileSelector(const QString &title = {});

    QString fileName() const;
    QString selectedDir() const { return _selectedDir; }

    void setFileName(const QString &fileName);
    void setSelectedDir(const QString &dir) { _selectedDir = dir; }
    void setFilters(const QList<QPair<QString, QString>> &filters);

    bool selectFile();

    QString verify();

private:
    QLabel *_statusLabel;
    QLabel *_statusIcon;
    QLineEdit *_fileName;
    QPushButton *_button;
    QString _selectedDir;
    QStringList _filters;
    QStringList _exts;

    void updateStatus();
};

#endif // FILE_SELECTOR_H
