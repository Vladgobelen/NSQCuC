#include "gamelauncher.h"
#include "settings.h"
#include <QFile>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QStandardPaths>
#include <QLoggingCategory>
#include <QProcess>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

Q_LOGGING_CATEGORY(launcherLog, "nsqcut.gamelauncher")

GameLauncher::GameLauncher(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_gameProcess(nullptr)
    , m_gameReady(false)
{
    qCDebug(launcherLog) << "GameLauncher initialized";
    checkGameStatus();
}

GameLauncher::~GameLauncher()
{
    qCDebug(launcherLog) << "GameLauncher destroyed";
    if (m_gameProcess && m_gameProcess->state() == QProcess::Running) {
        m_gameProcess->terminate();
    }
}

QString GameLauncher::getGamePath() const
{
    QString path = m_settings->gamePath();
    if (path.isEmpty()) {
#ifdef Q_OS_WIN
        QString defaultPath = "C:/Program Files (x86)/World of Warcraft";
#else
        QString defaultPath = QDir::homePath() + "/.wine/drive_c/Program Files (x86)/World of Warcraft";
#endif
        if (QDir(defaultPath).exists()) {
            return defaultPath;
        }
    }
    return path;
}

QString GameLauncher::getWowExePath() const
{
#ifdef Q_OS_WIN
    return getGamePath() + "/Wow.exe";
#else
    return getGamePath() + "/Wow.exe";
#endif
}

QString GameLauncher::getLogsPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/logs";
}

bool GameLauncher::isGameReady() const
{
    return m_gameReady;
}

void GameLauncher::checkGameStatus()
{
    bool ready = QFile::exists(getWowExePath());
    if (m_gameReady != ready) {
        m_gameReady = ready;
        emit gameStatusChanged(ready);
        qCDebug(launcherLog) << "Game status changed:" << (ready ? "ready" : "not found");
    }
}

bool GameLauncher::launch()
{
    QString gamePath = getGamePath();
    QString wowExe = getWowExePath();

    if (!QFile::exists(wowExe)) {
        qCWarning(launcherLog) << "Wow.exe not found:" << wowExe;
        return false;
    }

    qCDebug(launcherLog) << "Launching game from:" << gamePath;

#ifdef Q_OS_WIN
    // Windows: используем start для запуска в отдельном процессе
    bool success = QProcess::startDetached(
        "cmd",
        QStringList() << "/C" << "start" << "" << wowExe,
        gamePath
    );
#elif defined(Q_OS_LINUX)
    // Linux: через Wine
    bool success = QProcess::startDetached(
        "wine",
        QStringList() << wowExe,
        gamePath
    );
#elif defined(Q_OS_MACOS)
    qCWarning(launcherLog) << "macOS is not supported for WoW 3.3.5";
    return false;
#else
    bool success = false;
#endif

    if (success) {
        qCDebug(launcherLog) << "Game launched successfully";
    } else {
        qCWarning(launcherLog) << "Failed to launch game";
    }

    return success;
}

void GameLauncher::openLogsFolder()
{
    QString logsPath = getLogsPath();
    QDir().mkpath(logsPath);
    qCDebug(launcherLog) << "Opening logs folder:" << logsPath;
    QDesktopServices::openUrl(QUrl::fromLocalFile(logsPath));
}