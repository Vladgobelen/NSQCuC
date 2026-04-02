#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

class Settings;

class GameLauncher : public QObject
{
    Q_OBJECT

public:
    explicit GameLauncher(Settings *settings, QObject *parent = nullptr);
    ~GameLauncher() override;

    // === Публичные методы ===
    bool launch();
    void openLogsFolder();
    bool isGameReady() const;
    void checkGameStatus();

signals:
    // Статус игры изменился
    void gameStatusChanged(bool ready);

private:
    Settings *m_settings;
    QProcess *m_gameProcess;
    bool m_gameReady;

    // === Вспомогательные методы ===
    QString getGamePath() const;
    QString getWowExePath() const;
    QString getLogsPath() const;
};