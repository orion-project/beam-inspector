#include "AutoExposure.h"

#include "helpers/OriDialogs.h"

#include <QApplication>
#include <QClipboard>
#include <QDebug>

#define OPTIONAL(param)
#define REQUIRED(param) \
    if (!param) { \
        qWarning() << "AutoExposure: Parameter required:" << #param; \
        return false; \
    }

struct AutoExposureImpl
{
    AutoExposure* args;
    int stepCount, frameCount;
    double expoMin, expoMax, expoStep;
    double expo, expo1, expo2;
    double fps = 0;
    double levelAccum;
    QStringList logLines;
    QString logLine;

    AutoExposureImpl(AutoExposure* args) : args(args) {}
    
    class LogLine : public QTextStream
    {
    public:
        LogLine(const char* logId, QStringList &log, QString *line) 
            : QTextStream(line), logId(logId), log(log), line(line)
        {}
        
        ~LogLine()
        {
            qDebug().noquote() << logId << "AutoExpo:" << (*line);
            log << (*line);
        }
        
    private:
        const char* logId;
        QStringList& log;
        QString* line;
    };

    LogLine log()
    {
        logLine = {};
        return LogLine(args->logId, logLines, &logLine);
    }
    
    bool start()
    {
        log() << "target_level=" << args->targetLevel << " avg_frames=" << args->avgFrames;
        
        if (auto err = args->getExpoRange(&expoMin, &expoMax, &expoStep); !err.isEmpty()) {
            QString msg = QString("Failed to get exposure range: %1").arg(err);
            Ori::Dlg::Defer::error(msg);
            log() << msg;
            return false;
        }
        log() << "expo_min=" << expoMin << " expo_max=" << expoMax << " expo_step=" << expoStep;
        
        expo = expoMin;
        if (auto err = args->setExpo(expo); !err.isEmpty()){
            QString msg = QString("Failed to set exposure to %1: %2").arg(expo).arg(err);
            Ori::Dlg::Defer::error(msg);
            log() << msg;
            return false;
        }
        
        if (args->getFpsRange)
        {
            double fpsMin, fpsMax, fpsInc;
            if (auto err = args->getFpsRange(&fpsMin, &fpsMax, &fpsInc); !err.isEmpty()) {
                QString msg = QString("Failed to get FPS range: %1").arg(err);
                Ori::Dlg::Defer::error(msg);
                log() << msg;
                return false;
            }
            log() << "fps_min=" << fpsMin << " fps_max=" << fpsMax << " fps_inc=" << fpsInc;
            
            fps = fpsMax;
            if (auto err = args->setFps(fps); !err.isEmpty()) {
                QString msg = QString("Failed to set FPS to %1: %2").arg(fps).arg(err);
                Ori::Dlg::Defer::error(msg);
                log() << msg;
                return false;
            }
        }
        
        expo1 = expo;
        expo2 = 0;
        stepCount = 0;
        frameCount = 0;
        levelAccum = 0;
        args->stepRequested();
        return true;
    }
    
    bool setExpo(double v)
    {
        if (auto err = args->setExpo(v); !err.isEmpty()) {
            QString msg = QString("Failed to set exposure to %1: %2").arg(v).arg(err);
            Ori::Dlg::Defer::error(msg);
            log() << msg;
            return false;
        }

        if (auto err = args->getExpo(&expo); !err.isEmpty()) {
            QString msg = QString("Failed to get exposure after setting: %1").arg(err);
            Ori::Dlg::Defer::error(msg);
            log() << msg;
            return false;
        }

        if (args->getFps)
        {
            if (auto err = args->getFps(&fps); !err.isEmpty()) {
                QString msg = QString("Failed to get FPS after setting exposure: %1").arg(err);
                Ori::Dlg::Defer::error(msg);
                log() << msg;
                return false;
            }
        }

        if (args->expoChanged)
            args->expoChanged(expo, fps);

        return true;
    }
            
    void doStep(double level)
    {
        levelAccum += level;
        frameCount++;

        log() << "step=" << stepCount << " frame=" << frameCount
            << " expo=" << expo << " fps=" << fps << " level=" << level
            << " avg_level=" << levelAccum/double(frameCount);
            
        if (frameCount < args->avgFrames) {
            args->stepRequested();
            return;
        }
        
        double avgLevel = levelAccum / double(frameCount);

        if (qAbs(avgLevel - args->targetLevel) < 0.01) {
            log() << "stop_0=" << expo;
            goto stop;
        }
        
        if (avgLevel < args->targetLevel)
        {
            if (expo2 == 0)
            {
                if (!setExpo(qMin(expo1*2, expoMax)))
                    goto stop;

                // The above does not fail when setting higher-than-max exposure
                // It just clamps it to max and the loop never ends.
                // So need an explicit check:
                if (expo >= expoMax) {
                    log() << "underexposed=" << expo;
                    Ori::Dlg::Defer::warning(qApp->tr("Underexposed", "AutoExposure"));
                    goto stop;
                }

                expo1 = expo;
            }
            else
            {
                expo1 = expo;
                if (!setExpo((expo1+expo2)/2.0))
                    goto stop;

                if (qAbs(expo1 - expo) <= expoStep) {
                    log() << "stop_1 " << expo;
                    goto stop;
                }
            }
        }
        else
        {
            if (expo2 == 0)
            {
                if (expo <= expoMin) {
                    log() << "overexposed=" << expo;
                    Ori::Dlg::Defer::warning(qApp->tr("Overexposed", "AutoExposure"));
                    goto stop;
                }

                expo2 = expo1;
                expo1 /= 2.0;
                if (!setExpo((expo1+expo2)/2.0))
                    goto stop;
            }
            else
            {
                expo2 = expo;
                if (!setExpo((expo1+expo2)/2.0))
                    goto stop;

                if (qAbs(expo2 - expo) <= expoStep) {
                    log() << "stop_2=" << expo;
                    goto stop;
                }
            }
        }
        stepCount++;
        frameCount = 0;
        levelAccum = 0;
        args->stepRequested();
        return;
            
    stop:
        args->finished();
    }
};

bool AutoExposure::start()
{
    REQUIRED(logId)
    REQUIRED(targetLevel)
    REQUIRED(avgFrames)
    REQUIRED(getExpoRange)
    OPTIONAL(getFpsRange)
    REQUIRED(setExpo)
    REQUIRED(getExpo)
    OPTIONAL(setFps)
    OPTIONAL(getFps)
    REQUIRED(stepRequested)
    OPTIONAL(expoChanged)
    REQUIRED(finished)

    _impl.reset(new AutoExposureImpl(this));
    return _impl->start();
}

void AutoExposure::doStep(double level)
{
    _impl->doStep(level);
}

void AutoExposure::copyLog()
{
    qApp->clipboard()->setText(_impl->logLines.join('\n'));
}
