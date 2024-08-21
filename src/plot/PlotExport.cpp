#include "PlotExport.h"

#include "app/HelpSystem.h"

#include "widgets/FileSelector.h"

#include "helpers/OriDialogs.h"
#include "helpers/OriLayouts.h"
#include "helpers/OriWidgets.h"
#include "tools/OriSettings.h"
#include "widgets/OriPopupMessage.h"
#include "widgets/OriValueEdit.h"

#include "qcp/src/core.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>

using namespace Ori::Layouts;

//------------------------------------------------------------------------------
//                              ExportImageDlg
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
    ExportImageDlg(QCustomPlot* plot, const ExportImageProps& props) : plot(plot) {
        fileSelector = new FileSelector;
        fileSelector->setFileName(props.fileName);
        fileSelector->setFilters({
            { tr("PNG Images (*.png)"), "png" },
            { tr("JPG Images (*.jpg *.jpeg)"), "jpg" }
        });

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

        content = LayoutV({
            fileSelector,
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
            .withVerification([this]{ return fileSelector->verify(); })
            .withContentToButtonsSpacingFactor(2)
            .withPersistenceId("exportImageDlg")
            .withTitle(tr("Export Image"))
            .withAcceptTitle(tr("Save"))
            .withOnHelp([]{ HelpSystem::topic("export_plot"); })
            .exec();
    }

    void fillProps(ExportImageProps& props) {
        props.fileName = fileSelector->fileName();
        props.width = seWidth->value();
        props.height = seHeight->value();
        props.quality = seQuality->value();
        props.scale = edScale->value();
        props.proportional = cbProportional->isChecked();
    }

    QCustomPlot *plot;
    FileSelector *fileSelector;
    QSpinBox *seWidth, *seHeight, *seQuality;
    QCheckBox *cbProportional;
    Ori::Widgets::ValueEdit *edScale;
    QSharedPointer<QWidget> content;
    double aspect = 1;
    bool skipSizeChange = false;
};

void exportImageDlg(QCustomPlot* plot, std::function<void()> prepare, std::function<void()> unprepare)
{
    ExportImageProps props;
    ExportImageDlg dlg(plot, props);
    if (!dlg.exec())
        return;
    dlg.fillProps(props);
    props.save();
    prepare();
    bool ok = plot->saveRastered(props.fileName, props.width, props.height, props.scale, nullptr, props.quality);
    unprepare();
    if (!ok) {
        Ori::Gui::PopupMessage::error(qApp->translate("ExportImageDlg", "Failed to save image"));
        return;
    }
    Ori::Gui::PopupMessage::affirm(qApp->translate("ExportImageDlg", "Image saved to ") + props.fileName);
}

//------------------------------------------------------------------------------
//                              ExportRawImageDlg
//------------------------------------------------------------------------------

struct ExportRawImageProps
{
    QString fileName;

    ExportRawImageProps()
    {
        Ori::Settings s;
        s.beginGroup("ExportRawImage");
        fileName = s.value("fileName").toString();
    }

    void save()
    {
        Ori::Settings s;
        s.beginGroup("ExportRawImage");
        s.setValue("fileName", fileName);
    }
};

#define RAW_PREVIEW_W 600

class ExportRawImageDlg
{
    Q_DECLARE_TR_FUNCTIONS(ExportRawImageDlg)

public:
    ExportRawImageDlg(const QImage& img, const ExportRawImageProps& props) {
        fileSelector = new FileSelector;
        fileSelector->setFileName(props.fileName);
        fileSelector->setFilters({
            { tr("PNG Images (*.png)"), "png" },
            { tr("PGM Images (*.pgm)"), "pgm" },
            { tr("JPG Images (*.jpg *.jpeg)"), "jpg" }
        });

        double aspect = img.width() / (double)img.height();
        QPixmap pixmap(RAW_PREVIEW_W, RAW_PREVIEW_W/aspect);
        QPainter painter(&pixmap);
        painter.drawImage(pixmap.rect(), img, img.rect());
        auto preview = new QLabel;
        preview->setPixmap(pixmap);

        content = LayoutV({
            fileSelector,
            LayoutV({
                preview
            }).makeGroupBox(tr("Preview")),
        }).setMargin(0).makeWidgetAuto();
    }

    bool exec() {
        return Ori::Dlg::Dialog(content)
            .withVerification([this]{ return fileSelector->verify(); })
            .withContentToButtonsSpacingFactor(2)
            .withPersistenceId("exportRawImageDlg")
            .withTitle(tr("Export Raw Image"))
            .withAcceptTitle(tr("Save"))
            .withOnHelp([]{ HelpSystem::topic("export_raw"); })
            .exec();
    }

    void fillProps(ExportRawImageProps& props) {
        props.fileName = fileSelector->fileName();
    }

    FileSelector *fileSelector;
    QSharedPointer<QWidget> content;
};

void exportImageDlg(QByteArray data, int w, int h, bool hdr)
{
    QImage img((const uchar*)data.data(), w, h, hdr ? QImage::Format_Grayscale16 : QImage::Format_Grayscale8);
    ExportRawImageProps props;
    ExportRawImageDlg dlg(img, props);
    if (!dlg.exec())
        return;
    dlg.fillProps(props);
    props.save();
    bool ok = img.save(props.fileName);
    if (!ok) {
        Ori::Gui::PopupMessage::error(qApp->translate("ExportImageDlg", "Failed to save image"));
        return;
    }
    Ori::Gui::PopupMessage::affirm(qApp->translate("ExportImageDlg", "Image saved to ") + props.fileName);
}
