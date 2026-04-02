#pragma once

#include <QObject>
#include <QWebChannel>
#include <QWebEngineView>
#include <QNetworkAccessManager>
#include <QVariantMap>
#include <QJsonArray>

class AddonManager;
class Settings;
class GameLauncher;

class Backend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool gameReady READ gameReady NOTIFY gameReadyChanged)

public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend() override;

    void initialize(QWebEngineView *webView);

    Q_INVOKABLE QJsonArray loadAddons();
    Q_INVOKABLE void toggleAddon(const QString &name, bool install);
    Q_INVOKABLE QVariant checkGame();
    Q_INVOKABLE void changeGamePath(const QString &newPath);
    Q_INVOKABLE void setGamePath(const QString &path);
    Q_INVOKABLE QVariant launchGame();
    Q_INVOKABLE void openLogsFolder();
    Q_INVOKABLE QString openFileDialog(const QString &title, const QString &filter);

    bool gameReady() const;
    AddonManager* addonManager() const { return m_addonManager; }
    Settings* settings() const { return m_settings; }
    GameLauncher* gameLauncher() const { return m_gameLauncher; }

signals:
    void signalProgress(const QVariantMap &payload);
    void signalOperationFinished(const QVariantMap &payload);
    void signalOperationError(const QVariantMap &payload);
    void signalAddonInstallStarted(const QVariantMap &payload);
    void signalAddonInstallFinished(const QVariantMap &payload);
    void signalLaunchButtonState(const QVariantMap &payload);
    void gameReadyChanged();

private slots:
    void onAddonProgress(const QString &name, double progress);
    void onAddonOperationFinished(const QString &name, bool success);
    void onAddonOperationError(const QString &message);
    void onGameStatusChanged(bool ready);

private:
    QWebChannel *m_webChannel;
    QNetworkAccessManager *m_networkManager;
    AddonManager *m_addonManager;
    Settings *m_settings;
    GameLauncher *m_gameLauncher;
};