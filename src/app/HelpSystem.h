#ifndef HELP_SYSTEM_H
#define HELP_SYSTEM_H

#include <QObject>

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
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
    void showAbout();

private:
    HelpSystem();

    QNetworkAccessManager* _updateChecker = nullptr;
    QNetworkReply* _updateReply = nullptr;

    void versionReceived(QByteArray versionData) const;
};

#endif // HELP_SYSTEM_H
