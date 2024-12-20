#ifndef HELP_SYSTEM_H
#define HELP_SYSTEM_H

#include <QObject>

QT_BEGIN_NAMESPACE
#ifdef CHECK_UPDATES_WITH_CURL
class QProcess;
#else
class QNetworkAccessManager;
class QRestAccessManager;
#endif
QT_END_NAMESPACE

class HelpSystem : public QObject
{
    Q_OBJECT

public:
    static HelpSystem* instance();

    static QString appVersion();

    static void topic(const QString& topic) { instance()->showTopic(topic); }

public slots:
    void showContent();
    void showTopic(const QString& topic);
    void visitHomePage();
    void sendBugReport();
    void checkForUpdates();
    void checkForUpdatesAuto();
    void showAbout();

private:
    HelpSystem();

#ifdef CHECK_UPDATES_WITH_CURL
    QProcess* _updateChecker = nullptr;
#else
    QNetworkAccessManager* _updateChecker = nullptr;
    QRestAccessManager* _updateCheckerRest = nullptr;
#endif

    void getReleases(bool silent);
    void finishUpdateCheck();
};

#endif // HELP_SYSTEM_H
