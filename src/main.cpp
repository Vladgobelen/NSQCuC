#include <QApplication>
#include <QMainWindow>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebChannel>
#include <QUrl>
#include <QDir>
#include <QIcon>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QWebEngineSettings>
#include "backend.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

Q_LOGGING_CATEGORY(mainLog, "nsqcut.main")

class CspInterceptor : public QWebEngineUrlRequestInterceptor
{
public:
    void interceptRequest(QWebEngineUrlRequestInfo &info) override
    {
        if (info.requestUrl().host().contains("fiber-gate.ru")) {
            info.setHttpHeader("Content-Security-Policy",
                "default-src * 'unsafe-inline' 'unsafe-eval' data: blob: filesystem:; "
                "script-src * 'unsafe-inline' 'unsafe-eval'; "
                "style-src * 'unsafe-inline'; "
                "img-src * data: blob:; "
                "connect-src * wss: ws: https: http:; "
                "media-src * blob: data:; "
                "child-src * blob:; "
                "frame-src * blob:; "
                "worker-src * blob:; "
                "font-src * data:;");
            info.setHttpHeader("Access-Control-Allow-Origin", "*");
            info.setHttpHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            info.setHttpHeader("Access-Control-Allow-Headers", "*");
        }
    }
};

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    // Windows-specific flags
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
        "--disable-web-security "
        "--allow-running-insecure-content "
        "--disable-site-isolation-trials "
        "--disable-features=IsolateOrigins,site-per-process "
        "--autoplay-policy=no-user-gesture-required "
        "--disable-gpu "
        "--disable-gpu-compositing "
        "--no-sandbox "
        "--disable-dev-shm-usage "
        "--disable-blink-features=AutomationControlled "
        "--use-fake-ui-for-media-stream "
        "--enable-features=WebRTCPipeWireCapturer "
        "--disable-features=AudioServiceOutOfProcess "
        "--user-agent=\"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/111.0.0.0 Safari/537.36\"");
#else
    // Linux-specific flags
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
        "--disable-web-security "
        "--allow-running-insecure-content "
        "--disable-site-isolation-trials "
        "--disable-features=IsolateOrigins,site-per-process "
        "--autoplay-policy=no-user-gesture-required "
        "--disable-gpu "
        "--disable-gpu-compositing "
        "--no-sandbox "
        "--disable-setuid-sandbox "
        "--disable-dev-shm-usage "
        "--disable-blink-features=AutomationControlled "
        "--use-fake-ui-for-media-stream "
        "--enable-features=WebRTCPipeWireCapturer "
        "--alsa-input-device=default "
        "--alsa-output-device=default "
        "--disable-features=AudioServiceOutOfProcess "
        "--user-agent=\"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/111.0.0.0 Safari/537.36\"");
    qputenv("QT_QPA_PLATFORM", "xcb");
    qputenv("QTWEBENGINE_DISABLE_SANDBOX", "1");
    qputenv("PULSE_SERVER", "unix:/run/user/1000/pulse/native");
    qputenv("XDG_RUNTIME_DIR", QDir(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)).absolutePath().toUtf8());
#endif

    QLoggingCategory::setFilterRules(
        "nsqcut.*=true\n"
        "qt.webenginecontext.*=true");

    QApplication::setApplicationName("NSQCuT");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("NightWatch");

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle("Ночная стража - апдейтер");
    window.setMinimumSize(550, 650);
    window.resize(550, 650);
    window.setWindowIcon(QIcon(":/icons/icon.ico"));

    auto *webView = new QWebEngineView(&window);
    window.setCentralWidget(webView);

    auto *profile = webView->page()->profile();
    profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
    profile->setHttpUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/111.0.0.0 Safari/537.36 NightWatch/1.0");
    profile->setUrlRequestInterceptor(new CspInterceptor());
    profile->setPersistentStoragePath(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/webengine"
    );

    auto *settings = webView->page()->settings();
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    settings->setAttribute(QWebEngineSettings::AutoLoadImages, true);
    settings->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, true);
    settings->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);
    settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    settings->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, true);
    settings->setAttribute(QWebEngineSettings::PluginsEnabled, true);

    Backend backend;
    backend.initialize(webView);

    webView->setUrl(QUrl("qrc:/resources/index.html"));

    window.show();

    qCDebug(mainLog) << "Application started";

    return app.exec();
}