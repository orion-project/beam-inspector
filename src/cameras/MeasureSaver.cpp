#include "MeasureSaver.h"

#include "app/HelpSystem.h"
#include "cameras/Camera.h"
#include "cameras/CameraTypes.h"
#include "widgets/PlotHelpers.h"

#include "helpers/OriDialogs.h"
#include "helpers/OriLayouts.h"
#include "tools/OriSettings.h"
#include "widgets/FileSelector.h"
#include "widgets/OriPopupMessage.h"

#include <windows.h>

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
#include <QLockFile>
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
//                                   CsvFile
//------------------------------------------------------------------------------

struct CsvFile
{
    HANDLE hFile = INVALID_HANDLE_VALUE;
    QString error;
    
    bool open(const QString &fileName)
    {
        // Use WinAPI for locking the target file for writing
        // Other apps still can open it for reading
        hFile = CreateFileW(
            fileName.toStdWString().c_str(),
            FILE_APPEND_DATA | SYNCHRONIZE,
            FILE_SHARE_READ,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        if (hFile == INVALID_HANDLE_VALUE) {
            getSysError();
            return false;
        }
        return true;
    }
    
    bool close()
    {
        if (!CloseHandle(hFile)) {
            hFile = INVALID_HANDLE_VALUE;
            getSysError();
            return false;
        }
        hFile = INVALID_HANDLE_VALUE;
        return true;
    }
    
    bool writeLine(const QString &line)
    {
        QByteArray bytes = line.toUtf8();
        DWORD bytesWritten = 0;
        if (!WriteFile(hFile, bytes.data(), bytes.size(), &bytesWritten, NULL)) {
            getSysError();
            return false;
        }
        if (bytesWritten != bytes.size()) {
            error = QString("Written %1 of %2 bytes").arg(bytesWritten).arg(bytes.size());
            return false;
        }
        return true;
    }
    
    void getSysError()
    {
        DWORD err = GetLastError();
        const int bufSize = 4096;
        wchar_t buf[bufSize];
        auto size = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, 0, err, 0, buf, bufSize, 0);
        if (size == 0)
            error = QStringLiteral("code: 0x%1").arg(err, 0, 16);
        else
            error = QString::fromWCharArray(buf, size).trimmed();
    }
};

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
    _id = QUuid::createUuid().toString(QUuid::Id128);
    _measureStart = QDateTime::currentDateTime();
}

MeasureSaver::~MeasureSaver()
{
    _thread->quit();
    _thread->wait();
    qDebug() << LOG_ID << "Stopped";
    
    if (_csvFile) {
        if (!_csvFile->close())
            qWarning() << LOG_ID << "Error while closing results file" << _config.fileName << _csvFile->error;
        else qDebug() << LOG_ID << "Results file closed successfully" << _config.fileName;
    }
}

QString MeasureSaver::start(const MeasureConfig &cfg, Camera *cam)
{
    _config = cfg;
    _width = cam->width();
    _height = cam->height();
    _bpp = cam->bpp();
    _intervalBeg = -1;
    _intervalLen = _config.intervalSecs * 1000;
    _intervalIdx = 0;
    _avg_xc = 0, _avg_yc = 0;
    _avg_dx = 0, _avg_dy = 0;
    _avg_phi = 0;
    _avg_cnt = 0;
    _auxAvgVals = {};
    _auxAvgCnt = 0;
    _multires_avg.clear();
    _multires_avg_cnt.clear();
    _scale = cam->pixelScale().scaleFactor();

    if (auto res = checkConfig(); !res.isEmpty()) return res;
    if (auto res = acquireLock(); !res.isEmpty()) return res;
    if (auto res = prepareCsvFile(cam); !res.isEmpty()) return res;
    if (auto res = prepareImagesDir(); !res.isEmpty()) return res;
    if (auto res = saveIniFile(cam); !res.isEmpty()) return res;

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
    return {};
}

QString MeasureSaver::checkConfig()
{
    if (!_config.durationInf) {
        _duration = _config.durationSecs();
        if (_duration <= 0) {
            qCritical() << LOG_ID << "Invalid measurements duration" << _config.duration << "->" << _duration;
            return tr("Invalid measurements duration: %1").arg(_config.duration);
        }
    }

    if (_config.saveImg && parseDuration(_config.imgInterval) <= 0) {
        qCritical() << LOG_ID << "Invalid image save interval" << _config.imgInterval;
        return tr("Invalid image save interval: %1").arg(_config.imgInterval);
    }
    return QString();
}

QString MeasureSaver::acquireLock()
{
    _lockFile = std::unique_ptr<QLockFile>(new QLockFile(_config.fileName + ".lock"));
    _lockFile->setStaleLockTime(0);
    if (!_lockFile->tryLock()) {
        qCritical() << LOG_ID << "Results file is used by another running measurement" << _config.fileName;
        return tr("Results file is used by another running measurement");
    }
    return QString();
}

QString MeasureSaver::saveIniFile(Camera *cam)
{
    _cfgFile = _config.fileName;
    _cfgFile.replace(QFileInfo(_config.fileName).suffix(), "ini");
    QFile cfgFile(_cfgFile);
    if (!cfgFile.open(QIODeviceBase::WriteOnly | QIODeviceBase::Truncate)) {
        qCritical() << LOG_ID << "Failed to create configuration file" << _cfgFile << cfgFile.errorString();
        return tr("Failed to create configuration file:\n%1").arg(cfgFile.errorString());
    }
    cfgFile.close();

    qDebug() << LOG_ID << "Write settings" << _cfgFile;
    QSettings s(_cfgFile, QSettings::IniFormat);
    s.beginGroup(INI_GROUP_MEASURE);
    s.setValue("id", _id);
    s.setValue("timestamp", _measureStart.toString(Qt::ISODate));
    if (_config.saveImg)
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

    return QString();
}

QString MeasureSaver::prepareImagesDir()
{
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
    return QString();
}

QString MeasureSaver::prepareCsvFile(Camera *cam)
{
    const auto &camConfig = cam->config();

    qDebug() << LOG_ID << "Recreate target" << _config.fileName;
    _csvFile = std::unique_ptr<CsvFile>(new CsvFile);
    if (!_csvFile->open(_config.fileName)) {
        qCritical() << LOG_ID << "Failed to create results file" << _config.fileName << _csvFile->error;
        return tr("Failed to create results file:\n%1").arg(_csvFile->error);
    }

    // Write column headers
    QString headerLine;
    QTextStream out(&headerLine);
    out << "Index"
        << SEP << "Timestamp";
    if (camConfig.roiMode == ROI_MULTI) {
        _multires_cnt = camConfig.rois.size();
        for (int i = 0; i < _multires_cnt; i++) {
            const auto &roi = camConfig.rois.at(i);
            QString colSuffix = roi.label.isEmpty() ? QString("#%1").arg(i) : roi.label;
            out
                << SEP << "Center X (" << colSuffix << ')'
                << SEP << "Center Y (" << colSuffix << ')'
                << SEP << "Width X (" << colSuffix << ')'
                << SEP << "Width Y (" << colSuffix << ')'
                << SEP << "Azimuth (" << colSuffix << ')'
                << SEP << "Ellipticity (" << colSuffix << ')';
        }
    } else {
        _multires_cnt = 0;
        out
            << SEP << "Center X"
            << SEP << "Center Y"
            << SEP << "Width X"
            << SEP << "Width Y"
            << SEP << "Azimuth"
            << SEP << "Ellipticity";
    }
    _auxCols = {};
    const auto& cols = cam->measurCols();
    if (!cols.isEmpty()) {
        for (const auto &col : cols) {
            _auxCols << col.first;
            out << SEP << col.second;
        }
    }
    out << '\n';
    
    if (!_csvFile->writeLine(headerLine)) {
        qCritical() << LOG_ID << "Failed to write results file" << _config.fileName << _csvFile->error;
        return tr("Failed to write results file:\n%1").arg(_csvFile->error);
    }
    
    return QString();
}

#define OUT_VALS(xc, yc, dx, dy, phi, eps)              \
    out << SEP << QString::number(xc * _scale, 'f', 1)  \
        << SEP << QString::number(yc * _scale, 'f', 1)  \
        << SEP << QString::number(dx * _scale, 'f', 1)  \
        << SEP << QString::number(dy * _scale, 'f', 1)  \
        << SEP << QString::number(phi, 'f', 1)          \
        << SEP << QString::number(eps, 'f', 3);
        
#define OUT_AUX(aux)                                    \
    if (!_auxCols.empty()) {                            \
        for (const auto& id : std::as_const(_auxCols))  \
            if (id < MULTIRES_IDX)                      \
                out << SEP << aux.value(id);            \
    }                                                   \
    out << '\n';

#define OUT_TIME                                                               \
    out << _intervalIdx << SEP                                                 \
        << formatTime(r.time, Qt::ISODateWithMs);

#define OUT_ROW(nan, xc, yc, dx, dy, phi)                                      \
    if (nan) OUT_VALS(0, 0, 0, 0, 0, 0)                                        \
    else OUT_VALS(xc, yc, dx, dy, phi, EPS(dx, dy))


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
        int intervalIdx = _intervalIdx;
        for (int i = 0; i < e->count; i++) {
            const auto &r = e->results[i];
            OUT_ROW(r.nan, r.xc, r.yc, r.dx, r.dy, r.phi, r.eps(), r.cols);
            _intervalIdx++;
        }
        _intervalIdx = intervalIdx;
    }
    else qCritical() << "Failed to open check file" << checkFile.errorString();
#endif

    QString line;
    QTextStream out(&line);
    if (_config.allFrames)
    {
        for (int i = 0; i < e->count; i++) {
            const auto &r = e->results[i];
            OUT_TIME;
            if (_multires_cnt) {
                for (int j = 0; j < _multires_cnt; j++) {
                    const bool nan = r.cols[MULTIRES_IDX_NAN(j)] == 1;
                    const double xc = r.cols[MULTIRES_IDX_XC(j)];
                    const double yc = r.cols[MULTIRES_IDX_YC(j)];
                    const double dx = r.cols[MULTIRES_IDX_DX(j)];
                    const double dy = r.cols[MULTIRES_IDX_DY(j)];
                    const double phi = r.cols[MULTIRES_IDX_PHI(j)];
                    OUT_ROW(nan, xc, yc, dx, dy, phi);
                }
            } else {
                OUT_ROW(r.nan, r.xc, r.yc, r.dx, r.dy, r.phi);
            }
            OUT_AUX(r.cols);
            _intervalIdx++;
        }
    }
    else if (_config.average)
    {
        for (int i = 0; i < e->count; i++) {
            const auto &r = e->results[i];
            if (_multires_cnt) {
                for (int j = 0; j < _multires_cnt; j++) {
                    const bool nan = r.cols[MULTIRES_IDX_NAN(j)] == 1;
                    if (!nan) {
                        _multires_avg[MULTIRES_IDX_XC(j)] += r.cols[MULTIRES_IDX_XC(j)];
                        _multires_avg[MULTIRES_IDX_YC(j)] += r.cols[MULTIRES_IDX_YC(j)];
                        _multires_avg[MULTIRES_IDX_DX(j)] += r.cols[MULTIRES_IDX_DX(j)];
                        _multires_avg[MULTIRES_IDX_DY(j)] += r.cols[MULTIRES_IDX_DY(j)];
                        _multires_avg[MULTIRES_IDX_PHI(j)] += r.cols[MULTIRES_IDX_PHI(j)];
                        _multires_avg_cnt[j]++;
                    }
                }
            } else {
                if (!r.nan) {
                    _avg_xc += r.xc;
                    _avg_yc += r.yc;
                    _avg_dx += r.dx;
                    _avg_dy += r.dy;
                    _avg_phi += r.phi;
                    _avg_cnt++;
                }
            }
            if (!_auxCols.empty()) {
                for (const auto &id : std::as_const(_auxCols))
                    _auxAvgVals[id] += r.cols.value(id);
                _auxAvgCnt++;
            }

            if (i == 0) {
                _intervalBeg = r.time;
                continue;
            }

            if (i == e->count-1 || r.time - _intervalBeg >= _intervalLen) {
                if (!_auxCols.empty())
                    for (const auto &id : std::as_const(_auxCols))
                        if (id < MULTIRES_IDX)
                            _auxAvgVals[id] /= _auxAvgCnt;

                OUT_TIME;
                if (_multires_cnt) {
                    for (int j = 0; j < _multires_cnt; j++) {
                        const int avg_cnt = _multires_avg_cnt[j];
                        const double avg_xc = _multires_avg[MULTIRES_IDX_XC(j)];
                        const double avg_yc = _multires_avg[MULTIRES_IDX_YC(j)];
                        const double avg_dx = _multires_avg[MULTIRES_IDX_DX(j)];
                        const double avg_dy = _multires_avg[MULTIRES_IDX_DY(j)];
                        const double avg_phi = _multires_avg[MULTIRES_IDX_PHI(j)];
                        OUT_ROW(avg_cnt == 0,
                                avg_xc / avg_cnt,
                                avg_yc / avg_cnt,
                                avg_dx / avg_cnt,
                                avg_dy / avg_cnt,
                                avg_phi / avg_cnt
                                );
                    }
                    _multires_avg.clear();
                    _multires_avg_cnt.clear();
                } else {
                    OUT_ROW(_avg_cnt == 0,
                            _avg_xc / _avg_cnt,
                            _avg_yc / _avg_cnt,
                            _avg_dx / _avg_cnt,
                            _avg_dy / _avg_cnt,
                            _avg_phi / _avg_cnt
                            );
                    _avg_xc = 0, _avg_yc = 0;
                    _avg_dx = 0, _avg_dy = 0;
                    _avg_phi = 0;
                    _avg_cnt = 0;
                }
                OUT_AUX(_auxAvgVals);

                _intervalIdx++;
                _intervalBeg = r.time;
                _auxAvgVals = {};
                _auxAvgCnt = 0;
            }
        }
    }
    else
    {
        for (int i = 0; i < e->count; i++) {
            const auto &r = e->results[i];
            if (i == 0 || i == e->count-1 || r.time - _intervalBeg >= _intervalLen) {
                OUT_TIME;
                if (_multires_cnt) {
                    for (int j = 0; j < _multires_cnt; j++) {
                        const bool nan = r.cols[MULTIRES_IDX_NAN(j)] == 1;
                        const double xc = r.cols[MULTIRES_IDX_XC(j)];
                        const double yc = r.cols[MULTIRES_IDX_YC(j)];
                        const double dx = r.cols[MULTIRES_IDX_DX(j)];
                        const double dy = r.cols[MULTIRES_IDX_DY(j)];
                        const double phi = r.cols[MULTIRES_IDX_PHI(j)];
                        OUT_ROW(nan, xc, yc, dx, dy, phi);
                    }
                } else {
                    OUT_ROW(r.nan, r.xc, r.yc, r.dx, r.dy, r.phi);
                }
                OUT_AUX(r.cols);
                _intervalBeg = r.time;
                _intervalIdx++;
            }
        }
    }

    if (!_csvFile->writeLine(line)) {
        qCritical() << LOG_ID << "Failed to save resuls into file" << _config.fileName << _csvFile->error;
        emit interrupted(tr("Failed to save results into file") + '\n' + _config.fileName + '\n' + _csvFile->error);
        return;
    }

    auto elapsed = QDateTime::currentSecsSinceEpoch() - _measureStart.toSecsSinceEpoch();

    saveStats(e, elapsed);

    if (_duration > 0 and elapsed >= _duration)
        emit finished();
}

void MeasureSaver::saveStats(MeasureEvent *e, qint64 elapsed)
{
    QSettings s(_cfgFile, QSettings::IniFormat);
    s.beginGroup("Stats");
    s.setValue("elapsedTime", formatSecs(elapsed));
    s.setValue("resultsSaved", _intervalIdx);
    s.setValue("imagesSaved", _savedImgCount);
    for (auto it = e->stats.constBegin(); it != e->stats.constEnd(); it++)
        s.setValue(it.key(), it.value());
    s.endGroup();

    if (!_errors.isEmpty()) {
        s.beginGroup("Errors");
        for (auto it = _errors.constBegin(); it != _errors.constEnd(); it++) {
            QString key = formatTime(it.key(), Qt::ISODateWithMs);
            s.setValue(key, it.value());
        }
        _errors.clear();
        s.endGroup();
    }
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
        header << "P5\n" << _width << ' ' << _height << '\n' << (1<<_bpp)-1 << '\n';
    }
    if (_bpp > 8) {
        auto buf = (uint16_t*)e->buf.data();
        for (int i = 0; i < _width*_height; i++) {
            buf[i] = (buf[i] >> 8) | (buf[i] << 8);
        }
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
        fileSelector->setSaveDlg(true);

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
        labDuration->setForegroundRole(PlotHelpers::isDarkTheme() ? QPalette::Light : QPalette::Mid);

        rbSkipImg = new QRadioButton(tr("Don't save"));
        rbSaveImg = new QRadioButton(tr("Save every"));

        edImgInterval = new ShortLineEdit;
        edImgInterval->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred));
        edImgInterval->connect(edImgInterval, &QLineEdit::textChanged, edImgInterval, [this]{ updateImgIntervalSecs(); });

        labImgInterval = new QLabel;
        labImgInterval->setForegroundRole(PlotHelpers::isDarkTheme() ? QPalette::Light : QPalette::Mid);

        cbPresets = new QComboBox;
        cbPresets->setPlaceholderText(tr("Select a preset"));
        cbPresets->setToolTip(tr("Measurements preset"));
        cbPresets->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred));
        cbPresets->connect(cbPresets, &QComboBox::currentTextChanged, cbPresets, [this](const QString &t){ selectedPresetChnaged(t); });

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
        for (const auto &name : std::as_const(names))
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
            .withOnHelp([](){ HelpSystem::topic("measure"); })
            .windowModal()
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
