#include "HelpSystem.h"

#include "app/AppSettings.h"

#include "core/OriVersion.h"
#include "helpers/OriDialogs.h"
#include "helpers/OriLayouts.h"
#include "tools/OriHelpWindow.h"
#include "tools/OriSettings.h"
#include "widgets/OriLabels.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDateTime>
#include <QDialog>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QTimer>
#include <QUrl>
#ifdef CHECK_UPDATES_WITH_CURL
#include <QProcess>
#else
#include <QNetworkAccessManager>
#include <QRestAccessManager>
#include <QRestReply>
#endif

using namespace Ori::Layouts;
using Ori::Widgets::Label;

namespace {

HelpSystem *__instance = nullptr;

static QString homePageUrl() { return "https://github.com/orion-project/beam-inspector"; }
static QString newIssueUrl() { return "https://github.com/orion-project/beam-inspector/issues/new"; }

} // namespace

HelpSystem::HelpSystem() : QObject()
{
}

HelpSystem* HelpSystem::instance()
{
    if (!__instance)
        __instance = new HelpSystem();
    return __instance;
}

QString HelpSystem::appVersion()
{
    return QString("%1.%2.%3").arg(APP_VER_MAJOR).arg(APP_VER_MINOR).arg(APP_VER_PATCH);
}

void HelpSystem::showContent()
{
    Ori::HelpWindow::showContent();
}

void HelpSystem::showTopic(const QString& topic)
{
    Ori::HelpWindow::showTopic(topic);
}

void HelpSystem::visitHomePage()
{
    QDesktopServices::openUrl(QUrl(homePageUrl()));
}

void HelpSystem::sendBugReport()
{
    QDesktopServices::openUrl(QUrl(newIssueUrl()));
}

// https://docs.github.com/ru/rest/releases/releases?apiVersion=2022-11-28
void HelpSystem::checkForUpdates()
{
    getReleases(false);
}

void HelpSystem::checkForUpdatesAuto()
{
    Ori::Settings s;
    s.beginGroup("Update");
    auto interval = UpdateCheckInterval(s.value("checkInterval", int(UpdateCheckInterval::Weekly)).toInt());
    if (interval == UpdateCheckInterval::None)
        return;
    bool elapsed = false;
    auto lastTime = QDateTime::fromString(s.value("lastCheckTime").toString(), Qt::ISODate);
    if (!lastTime.isValid())
        elapsed = true;
    else {
        auto now = QDateTime::currentDateTime().date();
        auto prev = lastTime.date();
        switch (interval) {
        case UpdateCheckInterval::None:
            break;
        case UpdateCheckInterval::Daily:
            elapsed = now.dayOfYear() != prev.dayOfYear() || now.year() != prev.year();
            break;
        case UpdateCheckInterval::Weekly:
            elapsed = now.weekNumber() != prev.weekNumber() || now.year() != prev.year();
            break;
        case UpdateCheckInterval::Monthly:
            elapsed = now.month() != prev.month() || now.year() != prev.year();
            break;
        }
    }
    if (elapsed) {
        auto checkDelay = s.value("checkDelayMs", 300).toInt();
        if (checkDelay <= 0)
            checkDelay = 300;
        QTimer::singleShot(checkDelay, this, [this]{ getReleases(true); });
    }
}

namespace {
struct Release {
    Ori::Version ver;
    QDate date;
    QString descr;
};
}

void HelpSystem::getReleases(bool silent)
{
    #define LOG_ID "HelpSystem::getRelease:"
    if (_updateChecker)
    {
        qDebug() << LOG_ID << "Already in progress";
        return;
    }
#ifdef CHECK_UPDATES_WITH_CURL
/*
With QNetworkRequest it is unable to deal with such an error:

curl: (35) schannel: next InitializeSecurityContext failed: Unknown error (0x80092012) -
    The revocation function was unable to check revocation for the certificate.

QNetworkRequest hungs for a long time and then gets failed with error RemoteHostClosedError
With curl this can be ignored with --ssl-no-revoke (Disable cert revocation checks)

curl -L --ssl-no-revoke \
    -H "Accept: application/vnd.github+json" \
    -H "X-GitHub-Api-Version: 2022-11-28" \
    https://api.github.com/repos/orion-project/beam-inspector/releases
*/
    _updateChecker = new QProcess(this);
    _updateChecker->start("curl", { "-L", "--ssl-no-revoke",
        "-H", "Accept: application/vnd.github+json",
        "-H", "User-Agent: orion-project/beam-inspector-app",
        "-H", "X-GitHub-Api-Version: 2022-11-28",
        "https://api.github.com/repos/orion-project/beam-inspector/releases"
    });
    qDebug() << LOG_ID << "Started";
    connect(_updateChecker, &QProcess::finished, this, [this, silent](int exitCode, QProcess::ExitStatus exitStatus){
        if (exitStatus != QProcess::NormalExit) {
            qWarning() << LOG_ID << _updateChecker->errorString() << exitStatus;
            if (!silent)
                Ori::Dlg::error(_updateChecker->errorString());
            finishUpdateCheck();
            return;
        }
        if (exitCode != 0) {
            qWarning() << LOG_ID << "exitCode" << exitCode << QString::fromLocal8Bit(_updateChecker->readAllStandardError());
            if (!silent)
                Ori::Dlg::error(tr("Update check finished with error (%1)").arg(exitCode));
            finishUpdateCheck();
            return;
        }
        QJsonParseError err;
        auto doc = QJsonDocument::fromJson(_updateChecker->readAllStandardOutput(), &err);
#else
    _updateChecker = new QNetworkAccessManager(this);
    _updateCheckerRest = new QRestAccessManager(_updateChecker, this);
    QNetworkRequest request;
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setHeader(QNetworkRequest::UserAgentHeader, "orion-project/beam-inspector-app");
    request.setUrl(QUrl("https://api.github.com/repos/orion-project/beam-inspector/releases"));
    request.setTransferTimeout(5000);
    qDebug() << LOG_ID << "Started";
    _updateCheckerRest->get(request, this, [this, silent](QRestReply &reply){
        if (!reply.isSuccess()) {
            qWarning() << LOG_ID << reply.errorString() << reply.error();
            if (!silent)
                Ori::Dlg::error(reply.error() == QNetworkReply::OperationCanceledError ? QString("Operation timeout") : reply.errorString());
            finishUpdateCheck();
            return;
        }
        QJsonDocument doc;
        QJsonParseError err;
        if (auto d = reply.readJson(&err); d)
            doc = *d;
#endif
        if (err.error != QJsonParseError::NoError) {
            qWarning() << LOG_ID << err.errorString();
            if (!silent)
                Ori::Dlg::error(err.errorString());
            finishUpdateCheck();
            return;
        }
        if (!doc.isArray()) {
            qWarning() << LOG_ID << "Unexpected data format, json array expected";
            if (!silent)
                Ori::Dlg::error(tr("Server returned data in an unexpected format"));
            finishUpdateCheck();
            return;
        }
        auto arr = doc.array();
        QList<Release> releases;
        Ori::Version currentVersion(APP_VER_MAJOR, APP_VER_MINOR, APP_VER_PATCH);
        for (auto it = arr.constBegin(); it != arr.constEnd(); it++) {
            auto obj = it->toObject();
            Ori::Version version(obj["name"].toString());
            if (version > currentVersion)
                releases << Release {
                    .ver = version,
                    .date = QDate::fromString(obj["published_at"].toString(), Qt::ISODate),
                    .descr = obj["body"].toString(),
                };
        }
        if (releases.empty()) {
            qDebug() << LOG_ID << "No updates available";
            if (!silent)
                Ori::Dlg::info(tr("You are using the most recent version"));
            finishUpdateCheck();
            return;
        }
        std::sort(releases.begin(), releases.end(), [](Release& a, Release& b){ return a.ver > b.ver; });
        qDebug() << LOG_ID << "Update available" << currentVersion.str(3) << "->" << releases.first().ver.str(3);

        auto label = new QLabel(tr("A new version is available"));
        label->setAlignment(Qt::AlignHCenter);

        auto layout = new QFormLayout;
        layout->addRow(tr("Current version:"), new QLabel("<b>" + currentVersion.str(3) + "</b>"));
        layout->addRow(tr("New version:"), new QLabel("<b>" + releases.first().ver.str(3) + "</b>"));

        QString report;
        QTextStream stream(&report);
        for (auto r = releases.constBegin(); r != releases.constEnd(); r++)
            stream << "**" << r->ver.str(3) << "** (" << QLocale::system().toString(r->date, QLocale::ShortFormat) << ")\n\n" << r->descr << "\n\n";

        auto button = new QPushButton("      " + tr("Open download page") + "      ");
        connect(button, &QPushButton::clicked, this, []{
            QDesktopServices::openUrl(QUrl("https://github.com/orion-project/beam-inspector/releases"));
        });

        auto browser = new QTextBrowser;
        browser->setMarkdown(report);
        auto w = new QDialog;
        w->setAttribute(Qt::WA_DeleteOnClose);
        LayoutV({
            label,
            layout,
            new QLabel(tr("Changelog:")),
            browser,
            LayoutH({Stretch(), button, Stretch()}),
        }).useFor(w);
        w->resize(450, 400);
        w->exec();

        finishUpdateCheck();
    });
    #undef LOG_ID
}

void HelpSystem::finishUpdateCheck()
{
    Ori::Settings s;
    s.beginGroup("Update");
    s.setValue("lastCheckTime", QDateTime::currentDateTime().toString(Qt::ISODate));

    _updateChecker->deleteLater();
    _updateChecker = nullptr;

#ifndef CHECK_UPDATES_WITH_CURL
    _updateCheckerRest->deleteLater();
    _updateCheckerRest = nullptr;
#endif
}

void HelpSystem::showAbout()
{
    auto w = new QDialog;
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->setWindowTitle(tr("About %1").arg(qApp->applicationName()));

    QPixmap bg(":/about/bg");
    w->setMaximumSize(bg.size());
    w->setMinimumSize(bg.size());
    w->resize(bg.size());

    auto p = w->palette();
    p.setBrush(QPalette::Window, QBrush(bg));
    w->setPalette(p);

    auto labelVersion = new Label(qApp->applicationVersion());
    labelVersion->setStyleSheet("QLabel{font-weight:bold;font-size:50pt;color:#333344;padding-left:-40px}");
    labelVersion->setAlignment(Qt::AlignRight);
    labelVersion->setCursor(Qt::PointingHandCursor);
    labelVersion->setToolTip(tr("Check for updates"));
    connect(labelVersion, &Label::clicked, this, [this]{ getReleases(false); });

    auto labelDate = new Label(BUILD_DATE);
    labelDate->setStyleSheet("QLabel{font-size:20pt;color:#333344}");
    labelDate->setAlignment(Qt::AlignRight);
    labelDate->setCursor(Qt::PointingHandCursor);
    labelDate->setToolTip(tr("Check for updates"));
    connect(labelDate, &Label::clicked, this, [this]{ getReleases(false); });

    auto labelGit = new Label;
    labelGit->setPixmap(QIcon(":/about/github").pixmap(60));
    labelGit->setCursor(Qt::PointingHandCursor);
    labelGit->setToolTip(homePageUrl());
    connect(labelGit, &Label::clicked, this, [this]{ visitHomePage(); });

    auto labelQt = new Label;
    labelQt->setPixmap(QIcon(":/about/qt").pixmap(60));
    labelQt->setCursor(Qt::PointingHandCursor);
    labelQt->setToolTip(tr("About Qt"));
    connect(labelQt, &Label::clicked, this, []{ qApp->aboutQt(); });

    auto labelSupport1 = new Label;
    labelSupport1->setPixmap(QPixmap(":/about/n2-photonics"));
    labelSupport1->setCursor(Qt::PointingHandCursor);
    labelSupport1->setToolTip("https://www.n2-photonics.de");
    connect(labelSupport1, &Label::clicked, this, []{ QDesktopServices::openUrl(QUrl("https://www.n2-photonics.de")); });

    LayoutH({
        Space(bg.height()),
        LayoutV({
            labelVersion,
            labelDate,
            Stretch(),
            LayoutH({Stretch(), labelGit}).setMargin(0),
            SpaceV(1),
            LayoutH({Stretch(), labelQt}).setMargin(0),
            Stretch(),
            LayoutH({Stretch(), labelSupport1}).setMargin(0),
        }).setSpacing(0).setMargins(0, 0, 24, 16),
    }).setMargin(0).useFor(w);

    w->exec();
}
