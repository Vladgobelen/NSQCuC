#pragma once

#include <QObject>
#include <QSettings>
#include <QString>

class Settings : public QObject
{
    Q_OBJECT

public:
    explicit Settings(QObject *parent = nullptr);
    ~Settings() override;

    // === Геттеры/сеттеры ===
    QString gamePath() const;
    void setGamePath(const QString &path);
    
    QString pttHotkey() const;
    void setPttHotkey(const QString &hotkey);

    // === Сохранение/загрузка ===
    void load();
    void save();

    // === Утилиты ===
    static QString configPath();

private:
    QSettings *m_settings;
    QString m_gamePath;
    QString m_pttHotkey;
};