#include "addonmanager.h"
#include "settings.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QtConcurrent>
#include <QTimer>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QLoggingCategory>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

Q_LOGGING_CATEGORY(addonLog, "nsqcut.addonmanager")

AddonManager::AddonManager(QNetworkAccessManager *network,
                           Settings *settings,
                           QObject *parent)
    : QObject(parent)
    , m_network(network)
    , m_settings(settings)
    , m_processingCount(0)
{
    qCDebug(addonLog) << "AddonManager initialized";
}

AddonManager::~AddonManager()
{
    qCDebug(addonLog) << "AddonManager destroyed";
}

void AddonManager::setGamePath(const QString &path)
{
    m_gamePath = path;
    qCDebug(addonLog) << "Game path set to:" << path;
}

AddonInfo* AddonManager::findAddonByName(const QString &name)
{
    for (AddonInfo &info : m_addons) {
        if (info.name == name) {
            return &info;
        }
    }
    return nullptr;
}

QJsonArray AddonManager::loadAddons()
{
    qCDebug(addonLog) << "=== loadAddons START ===";
    const QString configUrl = "https://raw.githubusercontent.com/Vladgobelen/NSQCu/main/addons.json";
    qCDebug(addonLog) << "URL:" << configUrl;

    QUrl url{configUrl};
    QNetworkRequest request{url};
    request.setHeader(QNetworkRequest::UserAgentHeader, "NSQCuT/1.0");
    request.setRawHeader("Accept", "application/json");

    qCDebug(addonLog) << "Creating QNetworkReply...";
    QNetworkReply *reply = m_network->get(request);
    qCDebug(addonLog) << "Reply created:" << (reply ? "OK" : "NULL");

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    qCDebug(addonLog) << "Entering event loop...";
    bool loopResult = loop.exec(QEventLoop::ExcludeUserInputEvents);
    qCDebug(addonLog) << "Event loop exited, result:" << loopResult;

    if (!loopResult) {
        qCWarning(addonLog) << "Event loop returned false — possible abort or timeout";
    }

    if (reply->error() != QNetworkReply::NoError) {
        qCWarning(addonLog) << "Network error:" << reply->error() << reply->errorString();
        qCWarning(addonLog) << "HTTP status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        reply->deleteLater();
        return QJsonArray();
    }

    QByteArray data = reply->readAll();
    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qCDebug(addonLog) << "Received:" << data.size() << "bytes, HTTP:" << httpStatus;
    reply->deleteLater();

    if (httpStatus != 200) {
        qCWarning(addonLog) << "Unexpected HTTP status:" << httpStatus;
        return QJsonArray();
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qCWarning(addonLog) << "JSON parse error:" << parseError.errorString()
                            << "at offset" << parseError.offset;
        return QJsonArray();
    }

    // 🔥 Сохраняем полный конфиг в кэш
    m_addonsConfig = doc.object();
    qCDebug(addonLog) << "Config cached, root keys:" << m_addonsConfig.keys();

    QJsonArray resultArray;
    m_addons.clear();

    if (doc.isArray()) {
        qCDebug(addonLog) << "JSON is array - order preserved";
        resultArray = doc.array();
        for (int i = 0; i < resultArray.size(); ++i) {
            QJsonObject addonData = resultArray[i].toObject();
            addonData["installed"] = checkAddonInstalled(
                addonData.value("name").toString(),
                addonData.value("target_path").toString()
            );
            resultArray[i] = addonData;

            AddonInfo info;
            info.name = addonData.value("name").toString();
            info.description = addonData.value("description").toString();
            info.link = addonData.value("link").toString();
            info.targetPath = addonData.value("target_path").toString();
            info.isZip = addonData.contains("is_zip")
                ? addonData.value("is_zip").toBool()
                : info.link.toLower().endsWith(".zip");
            info.installed = addonData.value("installed").toBool(false);
            info.needsUpdate = addonData.value("needs_update").toBool(false);
            info.beingProcessed = false;
            info.updating = false;
            m_addons.append(info);
        }
    } else {
        qCDebug(addonLog) << "JSON is object - checking for addons key";
        QJsonObject rootObject = doc.object();
        QJsonValue addonsValue = rootObject.value("addons");

        if (addonsValue.isArray()) {
            qCDebug(addonLog) << "addons is array - order preserved";
            resultArray = addonsValue.toArray();
            for (int i = 0; i < resultArray.size(); ++i) {
                QJsonObject addonData = resultArray[i].toObject();
                addonData["installed"] = checkAddonInstalled(
                    addonData.value("name").toString(),
                    addonData.value("target_path").toString()
                );
                resultArray[i] = addonData;

                AddonInfo info;
                info.name = addonData.value("name").toString();
                info.description = addonData.value("description").toString();
                info.link = addonData.value("link").toString();
                info.targetPath = addonData.value("target_path").toString();
                info.isZip = addonData.contains("is_zip")
                    ? addonData.value("is_zip").toBool()
                    : info.link.toLower().endsWith(".zip");
                info.installed = addonData.value("installed").toBool(false);
                info.needsUpdate = addonData.value("needs_update").toBool(false);
                info.beingProcessed = false;
                info.updating = false;
                m_addons.append(info);
            }
        } else if (addonsValue.isObject()) {
            qCDebug(addonLog) << "addons is object - preserving order via regex";
            QJsonObject addonsObject = addonsValue.toObject();
            if (addonsObject.isEmpty()) {
                addonsObject = rootObject;
            }

            QString jsonText = QString::fromUtf8(data);
            QStringList orderedKeys;
            QRegularExpression re("\"([^\"]+)\"\\s*:\\s*\\{");
            QRegularExpressionMatchIterator it = re.globalMatch(jsonText);
            while (it.hasNext()) {
                QRegularExpressionMatch match = it.next();
                QString key = match.captured(1);
                if (key != "addons" && !orderedKeys.contains(key)) {
                    orderedKeys.append(key);
                }
            }

            for (const QString &key : orderedKeys) {
                if (addonsObject.contains(key)) {
                    QJsonObject addonData = addonsObject.value(key).toObject();
                    addonData["name"] = key;
                    addonData["installed"] = checkAddonInstalled(
                        key,
                        addonData.value("target_path").toString()
                    );
                    resultArray.append(addonData);

                    AddonInfo info;
                    info.name = key;
                    info.description = addonData.value("description").toString();
                    info.link = addonData.value("link").toString();
                    info.targetPath = addonData.value("target_path").toString();
                    info.isZip = addonData.contains("is_zip")
                        ? addonData.value("is_zip").toBool()
                        : info.link.toLower().endsWith(".zip");
                    info.installed = addonData.value("installed").toBool(false);
                    info.needsUpdate = addonData.value("needs_update").toBool(false);
                    info.beingProcessed = false;
                    info.updating = false;
                    m_addons.append(info);
                }
            }
        }
    }

    qCDebug(addonLog) << "=== loadAddons SUCCESS ===";
    qCDebug(addonLog) << "Total addons:" << resultArray.size();
    qCDebug(addonLog) << "m_addons count:" << m_addons.size();
    return resultArray;
}

bool AddonManager::checkAddonInstalled(const QString &name, const QString &targetPath)
{
    QString gamePath = m_gamePath.isEmpty() ? m_settings->gamePath() : m_gamePath;
    if (gamePath.isEmpty()) {
        qCDebug(addonLog) << "Game path empty, addon" << name << "not installed";
        return false;
    }
    QString fullPath = gamePath + "/" + targetPath;
    qCDebug(addonLog) << "Checking installed:" << name << "at" << fullPath;
    if (name == "NSQC") {
        QString versPath = fullPath + "/NSQC/vers";
        bool exists = QFile::exists(versPath);
        qCDebug(addonLog) << "NSQC vers check:" << versPath << "->" << exists;
        return exists;
    }
    else if (name == "NSQC3") {
        QString nsqc3Path = fullPath + "/NSQC3";
        bool exists = QDir(nsqc3Path).exists();
        qCDebug(addonLog) << "NSQC3 dir check:" << nsqc3Path << "->" << exists;
        return exists;
    }
    else {
        if (!QDir(fullPath).exists()) {
            return false;
        }
        QDirIterator it(fullPath, QDir::Files | QDir::Dirs, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString fileName = it.next();
            if (fileName.contains(name, Qt::CaseInsensitive)) {
                return true;
            }
        }
        return false;
    }
}

bool AddonManager::toggleAddon(const QString &name, bool install)
{
    qCDebug(addonLog) << "Toggle addon:" << name << "install:" << install;
    AddonInfo *addonPtr = findAddonByName(name);
    if (!addonPtr) {
        qCWarning(addonLog) << "Addon not found in m_addons:" << name;
        emit operationError("Addon not found: " + name);
        return false;
    }
    AddonInfo &addon = *addonPtr;
    m_processingCount++;
    addon.beingProcessed = true;
    addon.updating = true;
    bool success = false;
    if (install) {
        QEventLoop loop;
        installAddon(addon, [&loop, &success](bool result) {
            success = result;
            loop.quit();
        });
        loop.exec();
    }
    else {
        success = uninstallAddon(addon);
    }
    addon.beingProcessed = false;
    addon.updating = false;
    addon.installed = install ? success : !success;
    m_processingCount--;
    emit operationFinished(name, success);
    if (install && success && name == "NSQC") {
        autoUpdateNSQC3();
    }
    return success;
}

void AddonManager::installAddon(const AddonInfo &addon,
                                std::function<void(bool)> callback)
{
    qCDebug(addonLog) << "Installing addon:" << addon.name;
    emit progress(addon.name, 0.15);
    QNetworkRequest request(QUrl(addon.link));
    request.setHeader(QNetworkRequest::UserAgentHeader, "NightWatchUpdater/1.0");
    QNetworkReply *reply = m_network->get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, addon, reply, callback]() {
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(addonLog) << "Download error:" << reply->errorString();
            emit operationError("Failed to download: " + reply->errorString());
            reply->deleteLater();
            callback(false);
            return;
        }
        QByteArray data = reply->readAll();
        reply->deleteLater();
        qCDebug(addonLog) << "Downloaded" << data.size() << "bytes";
        emit progress(addon.name, 0.75);
        QString gamePath = m_gamePath.isEmpty() ? m_settings->gamePath() : m_gamePath;
        if (gamePath.isEmpty()) {
            emit operationError("Game path not set");
            callback(false);
            return;
        }
        QString targetDir = gamePath + "/" + addon.targetPath;
        QDir().mkpath(targetDir);
        bool success = false;
        if (addon.isZip) {
            success = extractZip(data, targetDir, addon.name);
        } else {
            QString fileName = QUrl(addon.link).fileName();
            if (fileName.isEmpty()) {
                fileName = addon.name;
            }
            QString filePath = targetDir + "/" + fileName;
            QFile file(filePath);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(data);
                file.close();
                success = true;
                qCDebug(addonLog) << "Saved binary file:" << filePath;
            } else {
                emit operationError("Failed to save file: " + file.errorString());
            }
        }
        emit progress(addon.name, 1.0);
        QTimer::singleShot(300, this, [this, addon, success, callback]() {
            if (success) {
                qCDebug(addonLog) << "Install completed:" << addon.name;
            }
            callback(success);
        });
    });
}

bool AddonManager::extractZip(const QByteArray &data,
                              const QString &targetDir,
                              const QString &addonName)
{
    QString tempDir = targetDir + "/_temp_extract";
    QDir().mkpath(tempDir);
    QString zipPath = tempDir + "/archive.zip";
    QFile zipFile(zipPath);
    if (!zipFile.open(QIODevice::WriteOnly)) {
        emit operationError("Failed to create temp zip file");
        return false;
    }
    zipFile.write(data);
    zipFile.close();
    QuaZip zip(zipPath);
    if (!zip.open(QuaZip::mdUnzip)) {
        emit operationError("Failed to open zip archive");
        return false;
    }
    QuaZipFile file(&zip);
    for (bool more = zip.goToFirstFile(); more; more = zip.goToNextFile()) {
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        QString fileName = file.getActualFileName();
        QString outPath = tempDir + "/" + fileName;
        if (fileName.endsWith('/')) {
            QDir().mkpath(outPath);
        } else {
            QDir().mkpath(QFileInfo(outPath).absolutePath());
            QFile outFile(outPath);
            if (outFile.open(QIODevice::WriteOnly)) {
                outFile.write(file.readAll());
                outFile.close();
            }
        }
        file.close();
    }
    zip.close();
    handleGitHubArchiveStructure(tempDir, targetDir, addonName);
    QDir(tempDir).removeRecursively();
    return true;
}

bool AddonManager::handleGitHubArchiveStructure(const QString &tempDir,
                                                const QString &targetDir,
                                                const QString &addonName)
{
    QStringList prefixes = {addonName + "-main", addonName + "-master"};
    QDirIterator it(tempDir, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::NoIteratorFlags);
    while (it.hasNext()) {
        QString dirPath = it.next();
        QString dirName = QFileInfo(dirPath).fileName();
        if (prefixes.contains(dirName)) {
            QString finalPath = targetDir + "/" + addonName;
            if (QDir(finalPath).exists()) {
                QDir(finalPath).removeRecursively();
            }
            if (QDir(dirPath).rename(dirPath, finalPath)) {
                qCDebug(addonLog) << "Renamed" << dirPath << "->" << finalPath;
                return true;
            }
        }
    }
    QDirIterator copyIt(tempDir, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
    while (copyIt.hasNext()) {
        QString src = copyIt.next();
        QString relPath = src.mid(tempDir.length() + 1);
        QString dst = targetDir + "/" + relPath;
        if (QFileInfo(src).isDir()) {
            QDir().mkpath(dst);
        } else {
            QDir().mkpath(QFileInfo(dst).absolutePath());
            QFile::copy(src, dst);
        }
    }
    return true;
}

bool AddonManager::uninstallAddon(const AddonInfo &addon)
{
    qCDebug(addonLog) << "Uninstalling addon:" << addon.name;
    QString gamePath = m_gamePath.isEmpty() ? m_settings->gamePath() : m_gamePath;
    if (gamePath.isEmpty()) {
        emit operationError("Game path not set");
        return false;
    }
    QString targetDir = gamePath + "/" + addon.targetPath;
    if (!QDir(targetDir).exists()) {
        qCDebug(addonLog) << "Target dir not exists, nothing to uninstall";
        return true;
    }
    QStringList itemsToRemove;
    QDirIterator it(targetDir, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString path = it.next();
        QString fileName = QFileInfo(path).fileName();
        if (fileName.contains(addon.name, Qt::CaseInsensitive)) {
            itemsToRemove.append(path);
        }
    }
    qCDebug(addonLog) << "Items to remove:" << itemsToRemove.size();
    for (int i = 0; i < itemsToRemove.size(); ++i) {
        double prog = 0.1 + 0.8 * ((i + 1) / (double)itemsToRemove.size());
        emit progress(addon.name, prog);
        const QString &path = itemsToRemove[i];
        if (QFileInfo(path).isDir()) {
            QDir(path).removeRecursively();
        } else {
            QFile::remove(path);
        }
    }
    emit progress(addon.name, 1.0);
    qCDebug(addonLog) << "Uninstall completed:" << addon.name;
    return true;
}

void AddonManager::checkForUpdates()
{
    startupUpdateCheck();
}

void AddonManager::startupUpdateCheck()
{
    qCDebug(addonLog) << "Startup update check";
    QString gamePath = m_gamePath.isEmpty() ? m_settings->gamePath() : m_gamePath;
    if (gamePath.isEmpty()) {
        return;
    }
    QString versPath = gamePath + "/Interface/AddOns/NSQC/vers";
    if (!QFile::exists(versPath)) {
        return;
    }
    QFile versFile(versPath);
    if (!versFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    QString localVersion = versFile.readAll().trimmed();
    versFile.close();
    if (localVersion.isEmpty()) {
        return;
    }
    qCDebug(addonLog) << "Local NSQC version:" << localVersion;
    const QString githubUrl = "https://github.com/Vladgobelen/NSQC/blob/main/vers";
    QUrl url{githubUrl};
    QNetworkRequest request{url};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    QNetworkReply *reply = m_network->get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, localVersion, gamePath, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            reply->deleteLater();
            return;
        }
        QString htmlContent = reply->readAll();
        reply->deleteLater();

        // 🔥 Удаляем HTML-теги для надёжного поиска
        QString plainText = htmlContent;
        plainText.remove(QRegularExpression("<[^>]*>"));

        bool found = plainText.contains(localVersion, Qt::CaseSensitive);

        qCDebug(addonLog) << "Version check - local:" << localVersion
                          << "found in plain text:" << found;

        if (found) {
            qCDebug(addonLog) << "Version" << localVersion << "found - up to date";
            return;
        }

        qCDebug(addonLog) << "Version" << localVersion << "NOT found - UPDATE REQUIRED!";
        forceReinstallAddons({"NSQC", "NSQC3"});
    });
}

QString AddonManager::htmlEscape(const QString &s)
{
    QString result = s;
    result.replace("&", "&amp;");
    result.replace("<", "&lt;");
    result.replace(">", "&gt;");
    result.replace("\"", "&quot;");
    result.replace("'", "&#39;");
    return result;
}

// 🔥 ИСПРАВЛЕНО: Возвращаем кэш, без сетевого запроса
QJsonObject AddonManager::fetchAddonsConfig()
{
    qCDebug(addonLog) << "=== fetchAddonsConfig (CACHED) ===";

    if (!m_addonsConfig.isEmpty()) {
        qCDebug(addonLog) << "Returning cached config";
        return m_addonsConfig;
    }

    qCWarning(addonLog) << "Config cache is empty — call loadAddons() first";
    return QJsonObject();
}

void AddonManager::forceReinstallAddons(const QStringList &addonNames)
{
    qCDebug(addonLog) << "Force reinstall:" << addonNames;

    // 🔥 Берём конфиг из кэша (уже загружен при старте)
    QJsonObject config = fetchAddonsConfig();
    if (config.isEmpty()) {
        qCWarning(addonLog) << "Config cache empty — reloading...";
        loadAddons();
        config = fetchAddonsConfig();
        if (config.isEmpty()) {
            emit operationError("Failed to load addons config");
            return;
        }
    }

    QJsonObject addons = config.value("addons").toObject();

    for (const QString &name : addonNames) {
        if (!addons.contains(name)) {
            qCWarning(addonLog) << "Addon" << name << "not found in config";
            continue;
        }

        QJsonObject addonData = addons.value(name).toObject();
        AddonInfo addon;
        addon.name = name;
        addon.description = addonData.value("description").toString();
        addon.link = addonData.value("link").toString();
        addon.targetPath = addonData.value("target_path").toString();
        addon.isZip = addonData.contains("is_zip")
            ? addonData.value("is_zip").toBool()
            : addon.link.toLower().endsWith(".zip");
        addon.installed = true;
        addon.beingProcessed = true;
        addon.updating = true;

        qCDebug(addonLog) << "Uninstalling" << name;
        uninstallAddon(addon);
        emit progress(name, 0.5);
        QThread::msleep(300);

        qCDebug(addonLog) << "Installing" << name;
        QEventLoop loop;
        installAddon(addon, [&loop](bool) { loop.quit(); });
        loop.exec();

        emit progress(name, 1.0);
        addon.beingProcessed = false;
        addon.updating = false;

        AddonInfo *existing = findAddonByName(name);
        if (existing) {
            *existing = addon;
        } else {
            m_addons.append(addon);
        }
        emit operationFinished(name, true);
        QThread::msleep(300);
    }

    loadAddons();
    qCDebug(addonLog) << "Reinstall completed";
}

void AddonManager::autoUpdateNSQC3()
{
    qCDebug(addonLog) << "Auto-update NSQC3 dependency";
    AddonInfo *nsqc3Ptr = findAddonByName("NSQC3");
    if (!nsqc3Ptr) {
        return;
    }
    AddonInfo &nsqc3 = *nsqc3Ptr;
    QString gamePath = m_gamePath.isEmpty() ? m_settings->gamePath() : m_gamePath;
    if (checkAddonInstalled("NSQC3", nsqc3.targetPath)) {
        qCDebug(addonLog) << "NSQC3 installed, uninstalling first";
        uninstallAddon(nsqc3);
    }
    qCDebug(addonLog) << "Installing fresh NSQC3";
    QEventLoop loop;
    installAddon(nsqc3, [&loop](bool) { loop.quit(); });
    loop.exec();
}