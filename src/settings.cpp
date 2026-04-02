#include "settings.h"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(settingsLog, "nsqcut.settings")

Settings::Settings(QObject *parent)
    : QObject(parent)
    , m_settings(nullptr)
{
    // Инициализация QSettings с организацией и именем приложения
    m_settings = new QSettings("NightWatch", "NSQCuT", this);
    qCDebug(settingsLog) << "Settings initialized, path:" << m_settings->fileName();
}

Settings::~Settings()
{
    qCDebug(settingsLog) << "Settings destroyed";
}

QString Settings::gamePath() const
{
    return m_gamePath;
}

void Settings::setGamePath(const QString &path)
{
    if (m_gamePath != path) {
        m_gamePath = path;
        m_settings->setValue("gamePath", path);
        qCDebug(settingsLog) << "Game path updated:" << path;
    }
}

QString Settings::pttHotkey() const
{
    return m_pttHotkey;
}

void Settings::setPttHotkey(const QString &hotkey)
{
    if (m_pttHotkey != hotkey) {
        m_pttHotkey = hotkey;
        m_settings->setValue("pttHotkey", hotkey);
    }
}

void Settings::load()
{
    qCDebug(settingsLog) << "Loading settings";
    
    m_gamePath = m_settings->value("gamePath").toString();
    m_pttHotkey = m_settings->value("pttHotkey").toString();
    
    qCDebug(settingsLog) << "Loaded gamePath:" << m_gamePath;
}

void Settings::save()
{
    qCDebug(settingsLog) << "Saving settings";
    
    m_settings->setValue("gamePath", m_gamePath);
    m_settings->setValue("pttHotkey", m_pttHotkey);
    m_settings->sync();
}

QString Settings::configPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) 
           + "/settings.json";
}