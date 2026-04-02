#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <QString>
#include <QByteArray>
#include <functional>

class Settings;

struct AddonInfo {
    QString name;
    QString description;
    bool installed;
    bool needsUpdate;
    bool beingProcessed;
    bool updating;
    QString link;
    QString targetPath;
    bool isZip;

    AddonInfo()
        : installed(false)
        , needsUpdate(false)
        , beingProcessed(false)
        , updating(false)
        , isZip(true)
    {}
};

class AddonManager : public QObject
{
    Q_OBJECT
public:
    explicit AddonManager(QNetworkAccessManager *network,
                          Settings *settings,
                          QObject *parent = nullptr);
    ~AddonManager() override;

    QJsonArray loadAddons();
    bool toggleAddon(const QString &name, bool install);
    void checkForUpdates();
    void startupUpdateCheck();
    void setGamePath(const QString &path);
    bool isProcessing() const { return m_processingCount > 0; }

signals:
    void progress(const QString &name, double value);
    void operationFinished(const QString &name, bool success);
    void operationError(const QString &message);

private:
    QNetworkAccessManager *m_network;
    Settings *m_settings;
    QList<AddonInfo> m_addons;
    QJsonObject m_addonsConfig;  // 🔥 Кэш конфига
    QString m_gamePath;
    int m_processingCount;

    bool checkAddonInstalled(const QString &name, const QString &targetPath);
    void installAddon(const AddonInfo &addon, std::function<void(bool)> callback);
    bool uninstallAddon(const AddonInfo &addon);
    bool handleGitHubArchiveStructure(const QString &tempDir,
                                      const QString &targetDir,
                                      const QString &addonName);
    bool copyDirRecursive(const QString &src, const QString &dst);
    QString htmlEscape(const QString &s);
    void forceReinstallAddons(const QStringList &addonNames);
    QJsonObject fetchAddonsConfig();
    void autoUpdateNSQC3();
    bool extractZip(const QByteArray &data,
                    const QString &targetDir,
                    const QString &addonName);
    AddonInfo* findAddonByName(const QString &name);
};