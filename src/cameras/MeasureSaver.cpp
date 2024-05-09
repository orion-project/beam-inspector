#include "MeasureSaver.h"

#include "cameras/Camera.h"
#include "cameras/CameraTypes.h"
#include "helpers/OriDialogs.h"
#include "helpers/OriLayouts.h"
#include "tools/OriSettings.h"
#include "widgets/FileSelector.h"

#include "helpers/OriDialogs.h"
#include "widgets/OriPopupMessage.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
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
#include <QToolBar>
#include <QUuid>

#define LOG_ID "MeasureSaver:"
#define INI_GROUP_MEASURE "Measurement"
#define INI_GROUP_PRESETS "MeasurePreset"
#define SEP ','
//#define SEP '\t'
//#define SAVE_CHECK_FILE

using namespace Ori::Layouts;

static int parseDuration(const QString &str)
{
    static QRegularExpression re("^\\s*((?<hour>\\d+)\\s*[hH])?(\\s*(?<min>\\d+)\\s*[mM])?(\\s*(?<sec>\\d+)\\s*[sS])?\\s*$");
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
    LOAD(duration, String, "5m");
    LOAD(saveImg, Bool, true);
    LOAD(imgInterval, String, "1m");
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
        if (saveImg)
            SAVE(imgInterval);
        else
            SAVE(saveImg);
    } else {
        SAVE(allFrames);
        SAVE(intervalSecs);
        SAVE(average);
        SAVE(durationInf);
        SAVE(duration);
        SAVE(saveImg);
        SAVE(imgInterval);
    }
}

void MeasureConfig::loadRecent()
{
    Ori::Settings s;
    s.beginGroup(INI_GROUP_MEASURE);
    load(s.settings());
}

void MeasureConfig::saveRecent() const
{
    Ori::Settings s;
    s.beginGroup(INI_GROUP_MEASURE);
    save(s.settings());
}

int MeasureConfig::durationSecs() const
{
    return parseDuration(duration);
}

int MeasureConfig::imgIntervalSecs() const
{
    return saveImg ? parseDuration(imgInterval) : 0;
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
    qDebug() << LOG_ID << "Stopped";
}

QString MeasureSaver::start(const MeasureConfig &cfg, Camera *cam)
{
    if (!cfg.durationInf) {
        _duration = cfg.durationSecs();
        if (_duration <= 0)
            return tr("Invalid measurements duration: %1").arg(cfg.duration);
    }

    if (cfg.saveImg && parseDuration(cfg.imgInterval) <= 0) {
        return tr("Invalid image save interval: %1").arg(cfg.imgInterval);
    }

    _config = cfg;
    _width = cam->width();
    _height = cam->height();
    _bits = cam->bits();

    auto scale = cam->pixelScale();
    _scale = scale.on ? scale.factor : 1;

    // Prepare images dir
    if (_config.saveImg) {
        QFileInfo fi(_config.fileName);
        QDir dir = fi.dir();
        QString subdir = fi.baseName() + ".images";
        _imgDir = dir.path() + '/' + subdir;
        if (dir.exists(subdir))
            qDebug() << LOG_ID << "Img dir exists" << _imgDir;
        else if (dir.mkdir(subdir))
            qDebug() << LOG_ID << "Img dir created" << _imgDir;
        else {
            qCritical() << LOG_ID << "Failed to create img dir" << _imgDir;
            return tr("Failed to create subdirectory for beam images");
        }
    }

    // Prepare config file
    _cfgFile = _config.fileName;
    _cfgFile.replace(QFileInfo(_config.fileName).suffix(), "ini");
    QFile cfgFile(_cfgFile);
    if (!cfgFile.open(QIODeviceBase::WriteOnly | QIODeviceBase::Truncate)) {
        qCritical() << LOG_ID << "Failed to create configuration file" << _cfgFile << cfgFile.errorString();
        return tr("Failed to create configuration file:\n%1").arg(cfgFile.errorString());
    }
    cfgFile.close();

    // Save measurement config
    qDebug() << LOG_ID << "Write settings" << _cfgFile;
    QSettings s(_cfgFile, QSettings::IniFormat);
    s.beginGroup(INI_GROUP_MEASURE);
    s.setValue("timestamp", QDateTime::currentDateTime().toString(Qt::ISODate));
    if (cfg.saveImg)
        s.setValue("imageDir", _imgDir);
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
    cam->saveHardConfig(&s);
    s.endGroup();

    s.beginGroup("CameraSettings");
    cam->config().save(&s, true);
    s.endGroup();

    // Prepare results file
    qDebug() << LOG_ID << "Recreate target" << _config.fileName;
    QFile csvFile(_config.fileName);
    if (!csvFile.open(QIODeviceBase::WriteOnly | QIODeviceBase::Truncate)) {
        qCritical() << LOG_ID << "Failed to create resuls file" << _config.fileName << csvFile.errorString();
        return tr("Failed to create resuls file:\n%1").arg(csvFile.errorString());
    }
    QTextStream out(&csvFile);
    out << "Index" << SEP
        << "Timestamp" << SEP
        << "Center X" << SEP
        << "Center Y" << SEP
        << "Width X" << SEP
        << "Width Y" << SEP
        << "Azimuth" << SEP
        << "Ellipticity" << '\n';
    csvFile.close();

#ifdef SAVE_CHECK_FILE
    QFile checkFile(_config.fileName + ".check");
    if (!checkFile.open(QIODeviceBase::WriteOnly | QIODeviceBase::Truncate)) {
        qCritical() << LOG_ID << "Failed to open check file" << checkFile.errorString();
    }
#endif

    _thread.reset(new QThread);
    moveToThread(_thread.get());
    _thread->start();
    qDebug() << LOG_ID << "Started" << QThread::currentThreadId();

    _measureStart = QDateTime::currentSecsSinceEpoch();

    _interval_beg = -1;
    _interval_len = _config.intervalSecs * 1000;
    _interval_idx = 0;
    _avg_xc = 0, _avg_yc = 0;
    _avg_dx = 0, _avg_dy = 0;
    _avg_phi = 0, _avg_eps = 0;
    _avg_cnt = 0;

    return {};
}

#define OUT_VALS(xc, yc, dx, dy, phi, eps)              \
    out << QString::number(xc * _scale, 'f', 1) << SEP  \
        << QString::number(yc * _scale, 'f', 1) << SEP  \
        << QString::number(dx * _scale, 'f', 1) << SEP  \
        << QString::number(dy * _scale, 'f', 1) << SEP  \
        << QString::number(phi, 'f', 1) << SEP          \
        << QString::number(eps, 'f', 3) << '\n'

#define OUT_ROW(nan, xc, yc, dx, dy, phi, eps)                                 \
    out << _interval_idx << SEP                                                \
        << _captureStart.addMSecs(r->time).toString(Qt::ISODateWithMs) << SEP; \
    if (nan) OUT_VALS(0, 0, 0, 0, 0, 0);                                       \
    else OUT_VALS(xc, yc, dx, dy, phi, eps)

bool MeasureSaver::event(QEvent *event)
{
    if (auto e = dynamic_cast<MeasureEvent*>(event); e) {
        processMeasure(e);
        return true;
    }
    if (auto e = dynamic_cast<ImageEvent*>(event); e) {
        saveImage(e);
        return true;
    }
    return QObject::event(event);
}

void MeasureSaver::processMeasure(MeasureEvent *e)
{
    qDebug() << LOG_ID << "Measurement" << e->num;

#ifdef SAVE_CHECK_FILE
    // Check file contains all results without splitting to intervals
    // To check if the splitting and averaging has been done correctly
    QFile checkFile(_config.fileName + ".check");
    if (checkFile.open(QIODeviceBase::WriteOnly | QIODeviceBase::Append)) {
        QTextStream out(&checkFile);
        int interval_idx = _interval_idx;
        for (auto r = e->results; r - e->results < e->count; r++) {
            OUT_ROW(r->nan, r->xc, r->yc, r->dx, r->dy, r->phi, r->eps());
            _interval_idx++;
        }
        _interval_idx = interval_idx;
    }
    else qCritical() << "Failed to open check file" << checkFile.errorString();
#endif

    QFile f(_config.fileName);
    if (!f.open(QIODeviceBase::WriteOnly | QIODeviceBase::Append)) {
        qCritical() << LOG_ID << "Failed to open resuls file" << _config.fileName << f.errorString();
        emit interrupted(tr("Failed to open resuls file") + '\n' + _config.fileName + '\n' + f.errorString());
        return;
    }

    QTextStream out(&f);
    for (auto r = e->results; r - e->results < e->count; r++) {
        if (_config.allFrames)
        {
            OUT_ROW(r->nan, r->xc, r->yc, r->dx, r->dy, r->phi, r->eps());
            _interval_idx++;
            continue;
        }

        if (_config.average and !r->nan) {
            _avg_xc += r->xc;
            _avg_yc += r->yc;
            _avg_dx += r->dx;
            _avg_dy += r->dy;
            _avg_phi += r->phi;
            _avg_eps += r->eps();
            _avg_cnt++;
        }

        if (_interval_beg < 0) {
            _interval_beg = r->time;

            // If we don't average, there is no need to wait
            // for the whole interval to get the first value
            if (!_config.average) {
                OUT_ROW(r->nan, r->xc, r->yc, r->dx, r->dy, r->phi, r->eps());
                _interval_idx++;
            }
            continue;
        }

        if (r->time - _interval_beg < _interval_len)
            continue;

        //qDebug() << _interval_idx << r->time << _interval_beg << _interval_len;

        if (!_config.average) {
            OUT_ROW(r->nan, r->xc, r->yc, r->dx, r->dy, r->phi, r->eps());
            _interval_idx++;
            _interval_beg = r->time;
            continue;
        }

        OUT_ROW(_avg_cnt == 0,
                _avg_xc / _avg_cnt,
                _avg_yc / _avg_cnt,
                _avg_dx / _avg_cnt,
                _avg_dy / _avg_cnt,
                _avg_phi / _avg_cnt,
                _avg_eps / _avg_cnt);

        _interval_idx++;
        _interval_beg = r->time;
        _avg_xc = 0, _avg_yc = 0;
        _avg_dx = 0, _avg_dy = 0;
        _avg_phi = 0, _avg_eps = 0;
        _avg_cnt = 0;
    }

    auto elapsed = QDateTime::currentSecsSinceEpoch() - _measureStart;

    QSettings s(_cfgFile, QSettings::IniFormat);
    s.beginGroup("Stats");
    s.setValue("elapsedTime", formatSecs(elapsed));
    s.setValue("resultsSaved", _interval_idx);
    s.setValue("imagesSaved", _savedImgCount);
    for (auto it = e->stats.constBegin(); it != e->stats.constEnd(); it++)
        s.setValue(it.key(), it.value());
    s.endGroup();

    if (!_errors.isEmpty()) {
        s.beginGroup("Errors");
        for (auto it = _errors.constBegin(); it != _errors.constEnd(); it++) {
            QString key = _captureStart.addMSecs(it.key()).toString(Qt::ISODateWithMs);
            s.setValue(key, it.value());
        }
        _errors.clear();
        s.endGroup();
    }

    if (_duration > 0 and elapsed >= _duration)
        emit finished();
}

void MeasureSaver::saveImage(ImageEvent *e)
{
    QString time = formatTime(e->time, QStringLiteral("yyyy-MM-ddThh-mm-ss-zzz"));
    QString path = _imgDir + '/' + time + ".pgm";
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        qWarning() << LOG_ID << "Failed to save image" << path << f.errorString();
        _errors.insert(e->time, "Failed to save image " + path + ": " + f.errorString());
        return;
    }
    {
        QTextStream header(&f);
        header << "P5\n" << _width << ' ' << _height << '\n' << (1<<_bits)-1 << '\n';
    }
    f.write(e->buf);
    _savedImgCount++;
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
        fileSelector->setFilters({{tr("CSV Files (*.csv)"), "csv"}});

        rbFramesAll = new QRadioButton(tr("Every frame"));
        rbFramesSec = new QRadioButton(tr("Given interval"));

        seFrameInterval = new QSpinBox;
        seFrameInterval->setMinimum(1);
        seFrameInterval->setMaximum(3600);
        seFrameInterval->setSuffix("s");

        cbAverageFrames = new QCheckBox(tr("With averaging"));

        rbDurationInf = new QRadioButton(tr("Until stop"));
        rbDurationSecs = new QRadioButton(tr("Given time"));

        edDuration = new ShortLineEdit;
        edDuration->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred));
        edDuration->connect(edDuration, &QLineEdit::textChanged, edDuration, [this]{ updateDurationSecs(); });

        labDuration = new QLabel;
        labDuration->setForegroundRole(qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark ? QPalette::Light : QPalette::Mid);

        rbSkipImg = new QRadioButton(tr("Don't save"));
        rbSaveImg = new QRadioButton(tr("Save every"));

        edImgInterval = new ShortLineEdit;
        edImgInterval->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred));
        edImgInterval->connect(edImgInterval, &QLineEdit::textChanged, edImgInterval, [this]{ updateImgIntervalSecs(); });

        labImgInterval = new QLabel;
        labImgInterval->setForegroundRole(qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark ? QPalette::Light : QPalette::Mid);

        cbPresets = new QComboBox;
        cbPresets->setPlaceholderText(tr("Select a preset"));
        cbPresets->setToolTip(tr("Measurements preset"));
        cbPresets->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred));
        cbPresets->connect(cbPresets, &QComboBox::currentTextChanged, [this](const QString &t){ selectedPresetChnaged(t); });

        auto toolbar = new QToolBar;
        auto color = toolbar->palette().brush(QPalette::Base).color();
        toolbar->setStyleSheet(QString("QToolBar{background:%1}").arg(color.name()));
        toolbar->addAction(QIcon(":/toolbar/open"), tr("Import config..."), [this]{ importConfig(); });
        toolbar->addAction(QIcon(":/toolbar/save"), tr("Export config..."), [this]{ exportConfig(); });
        toolbar->addSeparator();
        toolbar->addAction(QIcon(":/toolbar/star"), tr("Save preset..."), [this]{ saveConfigPreset(); });
        toolbar->addWidget(cbPresets);
        toolbar->addAction(QIcon(":/toolbar/trash"), tr("Delete preset"), [this]{ deleteConfigPreset(); });

        content = LayoutV({
            toolbar,
            LayoutV({
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
                    LayoutV({
                        rbSkipImg,
                        rbSaveImg,
                        edImgInterval,
                        labImgInterval,
                    }).makeGroupBox(tr("Raw images"))
                }),
            }).setDefSpacing(2).setDefMargins(),
        }).setSpacing(0).setMargin(0).makeWidgetAuto();

        fillPresetsList();
    }

    void populate(MeasureConfig &cfg)
    {
        fileSelector->setFileName(cfg.fileName);
        rbFramesAll->setChecked(cfg.allFrames);
        rbFramesSec->setChecked(!cfg.allFrames);
        seFrameInterval->setValue(cfg.intervalSecs);
        cbAverageFrames->setChecked(cfg.average);
        rbDurationInf->setChecked(cfg.durationInf);
        rbDurationSecs->setChecked(!cfg.durationInf);
        edDuration->setText(cfg.duration);
        rbSaveImg->setChecked(cfg.saveImg);
        rbSkipImg->setChecked(!cfg.saveImg);
        edImgInterval->setText(cfg.imgInterval);
        updateDurationSecs();
        updateImgIntervalSecs();
    }

    void collect(MeasureConfig &cfg) {
        cfg.fileName = fileSelector->fileName();
        cfg.allFrames = rbFramesAll->isChecked();
        cfg.intervalSecs = seFrameInterval->value();
        cfg.average = cbAverageFrames->isChecked();
        cfg.durationInf = rbDurationInf->isChecked();
        cfg.duration = edDuration->text().trimmed();
        cfg.saveImg = rbSaveImg->isChecked();
        cfg.imgInterval = edImgInterval->text().trimmed();
    }

    void updateDurationLabel(QLabel *label, QLineEdit *editor)
    {
        int secs = parseDuration(editor->text());
        if (secs <= 0)
            label->setText(QStringLiteral("<font color=red><b>%1</b></font>").arg(tr("Invalid time string")));
        else label->setText(QStringLiteral("<b>%1s<b>").arg(secs));
    }

    void updateDurationSecs() { updateDurationLabel(labDuration, edDuration); }
    void updateImgIntervalSecs() { updateDurationLabel(labImgInterval, edImgInterval); }

    void exportConfig()
    {
        Ori::Settings s;
        QString recentPath = s.value("recentMeasureCfgDir").toString();

        auto fileName = QFileDialog::getSaveFileName(
                                        qApp->activeModalWidget(),
                                        tr("Export Configuration"),
                                        recentPath,
                                        tr("INI Files (*.ini)"));
        if (fileName.isEmpty()) return;

        QFileInfo fi(fileName);
        if (fi.suffix().isEmpty())
            fileName += ".ini";

        s.setValue("recentMeasureCfgDir", fileName);

        // Need to manualy clear file content, because QSettings loads it
        QFile f(fileName);
        if (!f.open(QIODeviceBase::WriteOnly | QIODeviceBase::Truncate)) {
            qCritical() << LOG_ID << "Failed to create configuration file" << fileName << f.errorString();
            Ori::Dlg::error(tr("Failed to create configuration file:\n%1").arg(f.errorString()));
            return;
        }
        f.close();

        MeasureConfig cfg;
        collect(cfg);
        QSettings s1(fileName, QSettings::IniFormat);
        s1.beginGroup(INI_GROUP_MEASURE);
        cfg.save(&s1);

        Ori::Gui::PopupMessage::affirm(tr("Configuration exported"));
    }

    void importConfig()
    {
        Ori::Settings s;
        QString recentPath = s.value("recentMeasureCfgDir").toString();

        auto fileName = QFileDialog::getOpenFileName(
                                        qApp->activeModalWidget(),
                                        tr("Import Configuration"),
                                        recentPath,
                                        tr("INI Files (*.ini)"));
        if (fileName.isEmpty()) return;

        s.setValue("recentMeasureCfgDir", fileName);

        MeasureConfig cfg;
        QSettings s1(fileName, QSettings::IniFormat);
        s1.beginGroup(INI_GROUP_MEASURE);
        cfg.load(&s1);
        populate(cfg);
    }

    void saveConfigPreset()
    {
        QString name = Ori::Dlg::inputText(tr("Preset name:"), cbPresets->currentText());
        name = name.trimmed();
        if (name.isEmpty())
            return;
        Ori::Settings s;
        s.beginGroup(INI_GROUP_PRESETS);
        if (s.settings()->contains(name)) {
            if (!Ori::Dlg::yes(tr("Preset with this name already exists, overwrite?")))
                return;
        }
        QString group = s.value(name).toString();
        if (group.isEmpty()) {
            group = QUuid::createUuid().toString(QUuid::Id128);
            s.setValue(name, group);
        }
        s.beginGroup(group);
        MeasureConfig cfg;
        collect(cfg);
        cfg.save(s.settings());

        fillPresetsList(name);
        Ori::Gui::PopupMessage::affirm(tr("Preset saved"));
    }

    void deleteConfigPreset()
    {
        QString name = cbPresets->currentText();
        if (name.isEmpty()) {
            Ori::Gui::PopupMessage::warning(tr("Please select a preset in the drop-down"));
            return;
        }
        if (!Ori::Dlg::yes(tr("Preset will be deleted:<p><b>%1</b><p>Confirm?").arg(name)))
            return;

        Ori::Settings s;
        s.beginGroup(INI_GROUP_PRESETS);
        QString group = s.value(name).toString();
        s.settings()->remove(name);
        s.beginGroup(group);
        s.settings()->remove("");

        fillPresetsList();
        Ori::Gui::PopupMessage::affirm(tr("Preset deleted"));
    }

    void fillPresetsList(const QString &selected = {})
    {
        presetListUpdating = true;
        cbPresets->clear();
        Ori::Settings s;
        s.beginGroup(INI_GROUP_PRESETS);
        auto names = s.settings()->childKeys();
        names.sort();
        for (const auto &name : names)
            cbPresets->addItem(name, s.value(name));
        if (!selected.isEmpty())
            cbPresets->setCurrentText(selected);
        presetListUpdating = false;
    }

    void selectedPresetChnaged(const QString& name)
    {
        if (presetListUpdating) return;
        Ori::Settings s;
        s.beginGroup(INI_GROUP_PRESETS);
        QString group = s.value(name).toString();
        if (group.isEmpty()) return;
        MeasureConfig cfg;
        s.beginGroup(group);
        cfg.load(s.settings());
        populate(cfg);
    }

    bool exec() {
        return Ori::Dlg::Dialog(content)
            .withVerification([this]{
                auto fn = fileSelector->fileName();
                if (fn.isEmpty())
                    return tr("Target file not selected");
                if (!QFileInfo(fn).dir().exists())
                    return tr("Target directory does not exist");
                if (rbDurationSecs->isChecked())
                    if (parseDuration(edDuration->text()) <= 0)
                        return tr("Invalid measurement duration");
                if (rbSaveImg->isChecked())
                    if (parseDuration(edImgInterval->text()) <= 0)
                        return tr("Invalid image saving interval");
                return QString();
            })
            .withSkipContentMargins()
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
    QRadioButton *rbSkipImg, *rbSaveImg;
    QLineEdit *edImgInterval;
    QLabel *labImgInterval;
    QComboBox *cbPresets;
    bool presetListUpdating = false;
};

std::optional<MeasureConfig> MeasureSaver::configure()
{
    MeasureConfig cfg;
    cfg.loadRecent();
    MesureSaverDlg dlg(cfg);
    dlg.populate(cfg);
    if (!dlg.exec())
        return {};
    dlg.collect(cfg);
    cfg.saveRecent();
    return cfg;
}
