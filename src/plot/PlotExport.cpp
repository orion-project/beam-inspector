#include "PlotExport.h"

#include "helpers/OriDialogs.h"
#include "helpers/OriLayouts.h"
#include "helpers/OriWidgets.h"
#include "tools/OriSettings.h"
#include "widgets/OriValueEdit.h"

#include "qcp/src/core.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>

using namespace Ori::Layouts;

//------------------------------------------------------------------------------
//                              exportImageDlg
//------------------------------------------------------------------------------

struct ExportImageProps
{
    QString fileName;
    int width = 0;
    int height = 0;
    int quality = -1;
    double scale = 1;
    bool proportional = true;


    ExportImageProps()
    {
        Ori::Settings s;
        s.beginGroup("ExportImage");
        fileName = s.value("fileName").toString();
        width = s.value("width").toInt();
        height = s.value("height").toInt();
        quality = s.value("quality", -1).toInt();
        scale = s.value("scale", 1).toDouble();
        proportional = s.value("proportional", true).toBool();
    }

    void save()
    {
        Ori::Settings s;
        s.beginGroup("ExportImage");
        s.setValue("fileName", fileName);
        s.setValue("width", width);
        s.setValue("height", height);
        s.setValue("quality", quality);
        s.setValue("scale", scale);
        s.setValue("proportional", proportional);
    }
};

class ExportImageDlg
{
    Q_DECLARE_TR_FUNCTIONS(ExportImageDlg)

public:
    ExportImageDlg(QCustomPlot* plot, ExportImageProps& props) : plot(plot) {
        edFile = new QLineEdit;
        edFile->setText(props.fileName);
        edFile->connect(edFile, &QLineEdit::editingFinished, edFile, [this]{ updateFileStatus(); });

        labFileStatus = new QLabel;

        auto butSelectFile = new QPushButton(tr("Select..."));
        butSelectFile->connect(butSelectFile, &QPushButton::clicked, butSelectFile, [this]{ selectTargetFile(); });

        seWidth = Ori::Gui::spinBox(100, 10000, props.width > 0 ? props.width : plot->width());
        seHeight = Ori::Gui::spinBox(100, 10000, props.height > 0 ? props.height : plot->height());
        seWidth->connect(seWidth, QOverload<int>::of(&QSpinBox::valueChanged), seWidth, [this](int w){ widthChanged(w); });
        seHeight->connect(seHeight, QOverload<int>::of(&QSpinBox::valueChanged), seHeight, [this](int h){ heightChanged(h); });
        aspect = seWidth->value() / double(seHeight->value());

        auto sizeLayout = new QFormLayout;
        sizeLayout->addRow(tr("Widht"), seWidth);
        sizeLayout->addRow(tr("Height"), seHeight);

        cbProportional = new QCheckBox(tr("Proportional"));
        cbProportional->setChecked(props.proportional);

        seQuality = Ori::Gui::spinBox(-1, 100, (props.quality >= -1 && props.quality <= 100) ? props.quality : -1);

        edScale = new Ori::Widgets::ValueEdit;
        edScale->setPreferredWidth(seQuality->sizeHint().width());
        edScale->setValue(props.scale);

        auto scaleLayout = new QFormLayout;
        scaleLayout->addRow(tr("Scale"), edScale);
        scaleLayout->addRow(tr("Quality"), seQuality);

        auto butResetSize = new QPushButton("  " +  tr("Set size as on screen") + "  ");
        butResetSize->connect(butResetSize, &QPushButton::clicked, butResetSize, [this]{ resetImageSize(); });

        updateFileStatus();

        content = content = LayoutV({
            LayoutV({
                edFile,
                LayoutH({labFileStatus, SpaceH(2), Stretch(), butSelectFile}),
            }).makeGroupBox(tr("Target file")),
            LayoutV({
                LayoutH({
                    sizeLayout,
                    Stretch(),
                    scaleLayout,
                }),
                SpaceV(),
                LayoutH({
                    cbProportional,
                    Stretch(),
                    butResetSize,
                })
            }).makeGroupBox(tr("Image size")),
        }).setMargin(0).makeWidgetAuto();
    }

    void updateFileStatus() {
        QString fn = edFile->text().trimmed();
        if (fn.isEmpty()) {
            labFileStatus->setText(tr("File not selected"));
            return;
        }
        QFileInfo fi(fn);
        edFile->setText(fi.absoluteFilePath());
        if (fi.exists()) {
            labFileStatus->setText("<font color=orange>" + tr("File exists, will be overwritten") + "</font>");
            selectedDir = fi.dir().absolutePath();
        } else {
            auto dir = fi.dir();
            if (dir.exists()) {
                selectedDir = dir.absolutePath();
                labFileStatus->setText("<font color=green>" + tr("File not found, will be created") + "</font>");
            }
            else
                labFileStatus->setText("<font color=red>" + tr("Target directory does not exist") + "</font>");
        }
    }

    bool selectTargetFile() {
        const QStringList filters = {
            tr("PNG Images (*.png)"),
            tr("JPG Images (*.jpg *.jpeg)"),
        };
        const QStringList filterExts = { "png", "jpg" };
        Q_ASSERT(filters.size() == filterExts.size());
        QFileDialog dlg(qApp->activeModalWidget(), tr("Select a file name"), selectedDir);
        dlg.setNameFilters(filters);
        QString selectedFile = edFile->text().trimmed();
        if (!selectedFile.isEmpty()) {
            dlg.selectFile(selectedFile);
            QString ext = QFileInfo(selectedFile).suffix().toLower();
            for (const auto& f : filters) {
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
            for (int i = 0; i < filters.size(); i++)
                if (selectedFilter == filters.at(i)) {
                    selectedFile += "." + filterExts.at(i);
                    break;
                }
        }
        edFile->setText(selectedFile);
        updateFileStatus();
        return true;
    }

    void resetImageSize() {
        skipSizeChange = true;
        seWidth->setValue(plot->width());
        seHeight->setValue(plot->height());
        skipSizeChange = false;
    }

    void widthChanged(int w) {
        if (skipSizeChange) return;
        skipSizeChange = true;
        if (cbProportional->isChecked())
            seHeight->setValue(w / aspect);
        else aspect = w / seHeight->value();
        skipSizeChange = false;
    }

    void heightChanged(int h) {
        if (skipSizeChange) return;
        skipSizeChange = true;
        if (cbProportional->isChecked())
            seWidth->setValue(h * aspect);
        else aspect = seWidth->value() / double(h);
        skipSizeChange = false;
    }

    bool exec() {
        return Ori::Dlg::Dialog(content)
            .withVerification([this]{
                if (!QFileInfo(edFile->text()).dir().exists())
                    return tr("Target directory does not exist");
                return QString();
            })
            .withContentToButtonsSpacingFactor(2)
            .withPersistenceId("exportImageDlg")
            .withTitle(tr("Export Image"))
            .exec();
    }

    void fillProps(ExportImageProps& props) {
        props.fileName = edFile->text().trimmed();
        props.width = seWidth->value();
        props.height = seHeight->value();
        props.quality = seQuality->value();
        props.scale = edScale->value();
        props.proportional = cbProportional->isChecked();
    }

    QCustomPlot *plot;
    QLineEdit *edFile;
    QLabel *labFileStatus;
    QSpinBox *seWidth, *seHeight, *seQuality;
    QCheckBox *cbProportional;
    Ori::Widgets::ValueEdit *edScale;
    QSharedPointer<QWidget> content;
    double aspect = 1;
    bool skipSizeChange = false;
    QString selectedDir;
};

QString exportImageDlg(QCustomPlot* plot, std::function<void()> prepare, std::function<void()> unprepare)
{
    ExportImageProps props;
    ExportImageDlg dlg(plot, props);
    if (!dlg.exec())
        return {};
    dlg.fillProps(props);
    if (props.fileName.isEmpty())
    {
        if (!dlg.selectTargetFile())
            return {};
        dlg.fillProps(props);
    }
    props.save();
    prepare();
    bool ok = plot->saveRastered(props.fileName, props.width, props.height, props.scale, nullptr, props.quality);
    unprepare();
    if (!ok) {
        Ori::Dlg::error(qApp->translate("ExportImageDlg", "Failed to save image"));
        return {};
    }
    return props.fileName;
}
