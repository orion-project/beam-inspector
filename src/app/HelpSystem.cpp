#include "HelpSystem.h"

#include <QDesktopServices>
#include <QUrl>

namespace {

HelpSystem *__instance = nullptr;

static QString homePageUrl() { return "https://github.com/orion-project/cignus"; }
static QString newIssueUrl() { return "https://github.com/orion-project/cignus/issues/new"; }

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

void HelpSystem::visitHomePage()
{
    QDesktopServices::openUrl(QUrl(homePageUrl()));
}

void HelpSystem::sendBugReport()
{
    QDesktopServices::openUrl(QUrl(newIssueUrl()));
}
