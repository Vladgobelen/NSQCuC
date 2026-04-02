#include "backend.h"
#include "addonmanager.h"
#include "settings.h"
#include "gamelauncher.h"
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QFileDialog>
#include <QDir>
#include <QStandardPaths>
#include <QTimer>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QVariant>

Q_LOGGING_CATEGORY(backendLog, "nsqcut.backend")

Backend::Backend(QObject *parent)
    : QObject(parent)
    , m_webChannel(nullptr)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_addonManager(nullptr)
    , m_settings(nullptr)
    , m_gameLauncher(nullptr)
{
    qCDebug(backendLog) << "Backend constructor";
}

Backend::~Backend()
{
    qCDebug(backendLog) << "Backend destructor";
}

void Backend::initialize(QWebEngineView *webView)
{
    qCDebug(backendLog) << "Initializing backend with web view";
    auto *profile = webView->page()->profile();
    profile->setHttpUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/111.0.0.0 Safari/537.36 NightWatch/1.0");
    profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
    
    auto *settings = webView->page()->settings();
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    settings->setAttribute(QWebEngineSettings::AutoLoadImages, true);
    settings->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, true);
    settings->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);
    settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    settings->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, true);
    
    // 🔥 ОБРАБОТЧИК РАЗРЕШЕНИЙ — РАЗРЕШАЕМ ВСЁ ДЛЯ fiber-gate.ru
    connect(webView->page(), &QWebEnginePage::featurePermissionRequested,
        this, [webView](const QUrl &securityOrigin, QWebEnginePage::Feature feature) {
            qCDebug(backendLog) << "Permission requested:" << securityOrigin
                << "Feature:" << feature
                << "Scheme:" << securityOrigin.scheme()
                << "Host:" << securityOrigin.host();
            
            bool grantPermission = false;
            
            // 🔥 РАЗРЕШАЕМ ДЛЯ fiber-gate.ru И qrc/file схем
            if (securityOrigin.scheme() == "qrc" ||
                securityOrigin.scheme() == "file" ||
                securityOrigin.host().contains("fiber-gate.ru") ||
                securityOrigin.host().contains("ns.fiber-gate.ru") ||
                securityOrigin.host().contains("localhost") ||
                securityOrigin.host().contains("127.0.0.1")) {
                
                switch (feature) {
                    case QWebEnginePage::MediaAudioCapture:
                    case QWebEnginePage::MediaVideoCapture:
                    case QWebEnginePage::MediaAudioVideoCapture:
                    case QWebEnginePage::DesktopVideoCapture:
                    case QWebEnginePage::DesktopAudioVideoCapture:
                        grantPermission = true;
                        qCDebug(backendLog) << "Granting media permission for" << securityOrigin;
                        break;
                    default:
                        grantPermission = true;  // 🔥 Разрешаем всё остальное тоже
                        break;
                }
            } else {
                grantPermission = true;  // 🔥 Разрешаем все origins
            }
            
            if (grantPermission) {
                qCDebug(backendLog) << "Granting permission for" << securityOrigin
                    << "feature:" << feature;
                webView->page()->setFeaturePermission(
                    securityOrigin, feature, QWebEnginePage::PermissionGrantedByUser);
            } else {
                qCDebug(backendLog) << "Denying permission for" << securityOrigin
                    << "feature:" << feature;
                webView->page()->setFeaturePermission(
                    securityOrigin, feature, QWebEnginePage::PermissionDeniedByUser);
            }
        });
    
    m_webChannel = new QWebChannel(this);
    m_webChannel->registerObject(QStringLiteral("backend"), this);
    webView->page()->setWebChannel(m_webChannel);
    
    qCDebug(backendLog) << "WebChannel registered, backend object available";
    
    m_settings = new Settings(this);
    m_settings->load();
    
    m_addonManager = new AddonManager(m_networkManager, m_settings, this);
    m_gameLauncher = new GameLauncher(m_settings, this);
    
    connect(m_addonManager, &AddonManager::progress,
        this, &Backend::onAddonProgress);
    connect(m_addonManager, &AddonManager::operationFinished,
        this, &Backend::onAddonOperationFinished);
    connect(m_addonManager, &AddonManager::operationError,
        this, &Backend::onAddonOperationError);
    connect(m_gameLauncher, &GameLauncher::gameStatusChanged,
        this, &Backend::onGameStatusChanged);
    
    QTimer::singleShot(1000, this, [this]() {
        qCDebug(backendLog) << "Startup update check scheduled";
        if (m_addonManager) {
            m_addonManager->startupUpdateCheck();
        }
        auto *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]() {
            if (m_addonManager) {
                m_addonManager->checkForUpdates();
            }
        });
        timer->start(30000);
    });
}

bool Backend::gameReady() const
{
    return m_gameLauncher ? m_gameLauncher->isGameReady() : false;
}

QJsonArray Backend::loadAddons()
{
    qCDebug(backendLog) << "loadAddons called";
    if (!m_addonManager) {
        return QJsonArray();
    }
    return m_addonManager->loadAddons();
}

void Backend::toggleAddon(const QString &name, bool install)
{
    qCDebug(backendLog) << "toggleAddon:" << name << "install:" << install;
    if (!m_addonManager) {
        return;
    }
    QVariantMap startPayload;
    startPayload["name"] = name;
    startPayload["install"] = install;
    emit signalAddonInstallStarted(startPayload);
    
    auto *watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, name, watcher]() {
        bool success = watcher->result();
        QVariantMap finishPayload;
        finishPayload["name"] = name;
        finishPayload["success"] = success;
        emit signalAddonInstallFinished(finishPayload);
        QVariantMap btnState;
        btnState["enabled"] = !m_addonManager->isProcessing();
        emit signalLaunchButtonState(btnState);
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([this, name, install]() {
        return m_addonManager->toggleAddon(name, install);
    }));
}

QVariant Backend::checkGame()
{
    qCDebug(backendLog) << "checkGame called";
    bool ready = m_gameLauncher ? m_gameLauncher->isGameReady() : false;
    QVariantMap result;
    result["exists"] = ready;
    return result;
}

void Backend::changeGamePath(const QString &newPath)
{
    qCDebug(backendLog) << "changeGamePath newPath:" << newPath;
    if (!m_settings || !m_gameLauncher) {
        return;
    }
    QString wowPath = newPath + "/Wow.exe";
    if (!QFile::exists(wowPath)) {
        QVariantMap error;
        error["error"] = "Wow.exe not found in: " + newPath;
        emit signalOperationError(error);
        return;
    }
    m_settings->setGamePath(newPath);
    m_settings->save();
    if (m_addonManager) {
        m_addonManager->setGamePath(newPath);
    }
    m_gameLauncher->checkGameStatus();
}

void Backend::setGamePath(const QString &path)
{
    qCDebug(backendLog) << "setGamePath path:" << path;
    if (m_addonManager) {
        m_addonManager->setGamePath(path);
    }
}

QVariant Backend::launchGame()
{
    qCDebug(backendLog) << "launchGame called";
    if (!m_gameLauncher) {
        return QVariantMap{{"success", false}};
    }
    bool success = m_gameLauncher->launch();
    return QVariantMap{{"success", success}};
}

void Backend::openLogsFolder()
{
    qCDebug(backendLog) << "openLogsFolder called";
    if (m_gameLauncher) {
        m_gameLauncher->openLogsFolder();
    }
}

QString Backend::openFileDialog(const QString &title, const QString &filter)
{
    qCDebug(backendLog) << "openFileDialog:" << title << filter;
    QString fileName = QFileDialog::getOpenFileName(nullptr, title, QString(), filter);
    qCDebug(backendLog) << "openFileDialog result:" << fileName;
    return fileName;
}

void Backend::onAddonProgress(const QString &name, double progress)
{
    QVariantMap payload;
    payload["name"] = name;
    payload["progress"] = progress;
    emit signalProgress(payload);
}

void Backend::onAddonOperationFinished(const QString &name, bool success)
{
    QVariantMap payload;
    payload["name"] = name;
    payload["success"] = success;
    emit signalOperationFinished(payload);
}

void Backend::onAddonOperationError(const QString &message)
{
    QVariantMap payload;
    payload["message"] = message;
    emit signalOperationError(payload);
}

void Backend::onGameStatusChanged(bool ready)
{
    emit gameReadyChanged();
    QVariantMap payload;
    payload["enabled"] = ready && (!m_addonManager || !m_addonManager->isProcessing());
    emit signalLaunchButtonState(payload);
}