#include "MeasureSaver.h"

#include "cameras/Camera.h"
#include "cameras/CameraTypes.h"
#include "helpers/OriDialogs.h"
#include "helpers/OriLayouts.h"
#include "tools/OriSettings.h"
#include "widgets/FileSelector.h"

#include <QApplication>
#include <QCheckBox>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStyleHints>
#include <QThread>

using namespace Ori::Layouts;

static int parseDuration(const QString &str)
{
    static QRegularExpression re("^((?<hour>\\d+)\\s*[hH])?(\\s*(?<min>\\d+)\\s*[mM])?(\\s*(?<sec>\\d+)\\s*[sS])?$");
    auto match = re.match(str);
    if (!match.hasMatch())
        return -1;
    int h = match.captured("hour").toInt();
    int m = match.captured("min").toInt();
    int s = match.captured("sec").toInt();
    return 3600*h + 60*m + s;
}

//------------------------------------------------------------------------------
//                               MeasureConfig
//------------------------------------------------------------------------------

#define LOAD(option, type, def) option = s->value(QStringLiteral(#option), def).to ## type()
#define SAVE(option) s->setValue(QStringLiteral(#option), option)

void MeasureConfig::load(QSettings *s)
{
    LOAD(fileName, String, {});
    LOAD(allFrames, Bool, false);
    LOAD(intervalSecs, Int, 5);
    LOAD(average, Bool, true);
    LOAD(durationInf, Bool, false);
    LOAD(duration, String, "5 m");
}

void MeasureConfig::save(QSettings *s, bool min) const
{
    SAVE(fileName);
    if (min) {
        // In the min mode it stores only notable information
        // E.g. presence of intervalSecs should mean that allFrames disabled
        // But the config still must be correctly loadable,
        // so the default values must be adjusted respectively
        if (allFrames)
            SAVE(allFrames);
        else {
            SAVE(intervalSecs);
            SAVE(average);
        }
        if (durationInf)
            SAVE(durationInf);
        else
            SAVE(duration);
    } else {
        SAVE(allFrames);
        SAVE(intervalSecs);
        SAVE(average);
        SAVE(durationInf);
        SAVE(duration);
    }
}

void MeasureConfig::loadRecent()
{
    Ori::Settings s;
    s.beginGroup("Measure");
    load(s.settings());
}

void MeasureConfig::saveRecent() const
{
    Ori::Settings s;
    s.beginGroup("Measure");
    save(s.settings());
}

//------------------------------------------------------------------------------
//                               MeasureSaver
//------------------------------------------------------------------------------

MeasureSaver::MeasureSaver() : QObject()
{
}

MeasureSaver::~MeasureSaver()
{
    _thread->quit();
    _thread->wait();
    qDebug() << "MeasureSaver: stopped";
}

QString MeasureSaver::start(const MeasureConfig &cfg, Camera *cam)
{
    if (!cfg.durationInf) {
        _duration = parseDuration(cfg.duration);
        if (_duration <= 0)
            return tr("Invalid duration string: %1").arg(cfg.duration);
    }

    _config = cfg;

    auto scale = cam->pixelScale();
    _scale = scale.on ? scale.factor : 1;

    QString cfgFileName = _config.fileName;
    cfgFileName.replace(QFileInfo(_config.fileName).suffix(), "cfg");
    QFile cfgFile(cfgFileName);
    if (!cfgFile.open(QIODeviceBase::WriteOnly | QIODeviceBase::Truncate))
        return tr("Failed to create configuration file:\n%1").arg(cfgFile.errorString());
    cfgFile.close();

    qDebug() << "MeasureSaver: write settings" << cfgFileName;
    QSettings s(cfgFileName, QSettings::IniFormat);
    s.beginGroup("Measurement");
    s.setValue("timestamp", QDateTime::currentDateTime().toString(Qt::ISODate));
    _config.save(&s, true);
    s.endGroup();

    auto sensorScale = cam->sensorScale();
    s.beginGroup("Camera");
    s.setValue("name", cam->name());
    s.setValue("resolution", cam->resolutionStr());
    s.setValue("sensorScale.on", sensorScale.on);
    if (sensorScale.on) {
        s.setValue("sensorScale.factor", sensorScale.factor);
        s.setValue("sensorScale.unit", sensorScale.unit);
    }
    s.endGroup();

    s.beginGroup("CameraSettings");
    cam->config().save(&s, true);
    s.endGroup();

    qDebug() << "MeasureSaver: recreate target" << _config.fileName;
    QFile csvFile(_config.fileName);
    if (!csvFile.open(QIODeviceBase::WriteOnly | QIODeviceBase::Truncate))
        return tr("Failed to create resuls file:\n%1").arg(csvFile.errorString());
    QTextStream out(&csvFile);
    out << "Index,Timestamp,Center X,Center Y,Width X,Width Y,Azimuth,Ellipticity\n";

    _thread.reset(new QThread);
    moveToThread(_thread.get());
    _thread->start();
    qDebug() << "MeasureSaver: started" << QThread::currentThreadId();

    _startTime = QDateTime::currentSecsSinceEpoch();

    return {};
}

bool MeasureSaver::event(QEvent *event)
{
    auto e = dynamic_cast<MeasureEvent*>(event);
    if (!e) return QObject::event(event);

    qDebug() << "MeasureSaver: measurement" << e->num;

    QFile f(_config.fileName);
    if (!f.open(QIODeviceBase::WriteOnly | QIODeviceBase::Append)) {
        qCritical() << "Failed to open resuls file" << f.errorString();
        return true;
    }

    QTextStream out(&f);
    for (auto r = e->results; r - e->results < e->count; r++) {
        out << r->idx << ','
            << e->start.addMSecs(r->time).toString(Qt::ISODateWithMs) << ',';
        if (r->nan)
            out << 0 << ',' // xc
                << 0 << ',' // yc
                << 0 << ',' // dx
                << 0 << ',' // dy
                << 0 << ',' // phi
                << 0;       // eps
        else
            out << int(r->xc * _scale) << ','
                << int(r->yc * _scale) << ','
                << int(r->dx * _scale) << ','
                << int(r->dy * _scale) << ','
                << QString::number(r->phi, 'f', 1) << ','
                << QString::number(qMin(r->dx, r->dy) / qMax(r->dx, r->dy), 'f', 3);
        out << '\n';
    }

    if (_duration > 0)
        if (QDateTime::currentSecsSinceEpoch() - _startTime >= _duration)
            emit finished();

    return true;
}

//------------------------------------------------------------------------------
//                               MesureSaverDlg
//------------------------------------------------------------------------------

class ShortLineEdit : public QLineEdit
{
    QSize sizeHint() const override { return {50, QLineEdit::sizeHint().height()}; }
};

class MesureSaverDlg
{
    Q_DECLARE_TR_FUNCTIONS(MesureSaverDlg)

public:
    MesureSaverDlg(const MeasureConfig &cfg) {
        fileSelector = new FileSelector;
        fileSelector->setFileName(cfg.fileName);
        fileSelector->setFilters({{tr("CSV Files (*.csv)"), "csv"}});

        rbFramesAll = new QRadioButton(tr("Every frame"));
        rbFramesAll->setChecked(cfg.allFrames);

        rbFramesSec = new QRadioButton(tr("Given interval"));
        rbFramesSec->setChecked(!cfg.allFrames);

        seFrameInterval = new QSpinBox;
        seFrameInterval->setMinimum(1);
        seFrameInterval->setMaximum(3600);
        seFrameInterval->setValue(cfg.intervalSecs);
        seFrameInterval->setSuffix(" s");

        cbAverageFrames = new QCheckBox(tr("With averaging"));
        cbAverageFrames->setChecked(cfg.average);

        rbDurationInf = new QRadioButton(tr("Until stop"));
        rbDurationInf->setChecked(cfg.durationInf);

        rbDurationSecs = new QRadioButton(tr("Given time"));
        rbDurationSecs->setChecked(!cfg.durationInf);

        edDuration = new ShortLineEdit;
        edDuration->setText(cfg.duration);
        edDuration->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred));
        edDuration->connect(edDuration, &QLineEdit::textChanged, edDuration, [this]{ updateDurationSecs(); });

        labDuration = new QLabel;
        labDuration->setForegroundRole(qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark ? QPalette::Light : QPalette::Mid);
        updateDurationSecs();

        content = content = LayoutV({
            fileSelector,
            LayoutH({
                LayoutV({
                    rbFramesAll,
                    rbFramesSec,
                    seFrameInterval,
                    cbAverageFrames,
                }).makeGroupBox(tr("Interval")),
                LayoutV({
                    rbDurationInf,
                    rbDurationSecs,
                    edDuration,
                    labDuration,
                }).makeGroupBox(tr("Duration")),
            }),
        }).setMargin(0).makeWidgetAuto();
    }

    void fillProps(MeasureConfig &cfg) {
        cfg.fileName = fileSelector->fileName();
        cfg.allFrames = rbFramesAll->isChecked();
        cfg.intervalSecs = seFrameInterval->value();
        cfg.average = cbAverageFrames->isChecked();
        cfg.durationInf = rbDurationInf->isChecked();
        cfg.duration = edDuration->text().trimmed();
    }

    void updateDurationSecs()
    {
        int secs = parseDuration(edDuration->text().trimmed());
        if (secs <= 0)
            labDuration->setText(QStringLiteral("<font color=red><b>%1</b></font>").arg(tr("Invalid time string")));
        else labDuration->setText(QStringLiteral("<b>%1 s<b>").arg(secs));
    }


    bool exec() {
        return Ori::Dlg::Dialog(content)
            .withVerification([this]{
                auto fn = fileSelector->fileName();
                if (fn.isEmpty())
                    return tr("Target file not selected");
                if (!QFileInfo(fn).dir().exists())
                    return tr("Target directory does not exist");
                return QString();
            })
            .withContentToButtonsSpacingFactor(2)
            .withPersistenceId("measureCfgDlg")
            .withTitle(tr("Measurement Configuration"))
            .withAcceptTitle(tr("Start"))
            .exec();
    }

    FileSelector *fileSelector;
    QSharedPointer<QWidget> content;
    QRadioButton *rbFramesAll, *rbFramesSec;
    QSpinBox *seFrameInterval;
    QCheckBox *cbAverageFrames;
    QRadioButton *rbDurationInf, *rbDurationSecs;
    QLineEdit *edDuration;
    QLabel *labDuration;
};

std::optional<MeasureConfig> MeasureSaver::configure()
{
    MeasureConfig cfg;
    cfg.loadRecent();
    MesureSaverDlg dlg(cfg);
    if (!dlg.exec())
        return {};
    dlg.fillProps(cfg);
    cfg.saveRecent();
    return cfg;
}
